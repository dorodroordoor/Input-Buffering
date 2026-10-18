#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

// Shared state for the "what if" simulation
namespace sim {
	inline bool  active = false;  // currently simulating
	inline bool  died   = false;  // player died during the simulation
	inline float dt     = 1.f / 240.f;
}

// During a simulation, swallow the death instead of actually killing the player
class $modify(SimPlayer, PlayerObject) {
	void playerDestroyed(bool p0) {
		if (sim::active) {
			sim::died = true;
			return;
		}
		PlayerObject::playerDestroyed(p0);
	}
};

class $modify(InputBuffer, GJBaseGameLayer) {
	struct Pending {
		bool active = false;
		bool down   = false;
		int  left   = 0;
	};
	struct Fields {
		Pending pending[2];
	};

	// Copy the player's state, apply the input on tick `delay`, then keep going for
	// `lookahead` more ticks after it. Report whether the player survived, then restore.
	bool survives(PlayerObject* p, bool down, int delay, int lookahead) {
		auto cp = PlayerCheckpoint::create();
		p->saveToCheckpoint(cp);
		bool wasHolding = p->m_isHolding;

		sim::active = true;
		sim::died = false;
		for (int t = 0; t < delay + lookahead && !sim::died; t++) {
			if (t == delay) {
				if (down) p->pushButton(PlayerButton::Jump);
				else      p->releaseButton(PlayerButton::Jump);
			}
			p->update(sim::dt);
			this->checkCollisions(p, sim::dt, false);
		}
		sim::active = false;
		bool ok = !sim::died;

		p->loadFromCheckpoint(cp);
		p->m_isHolding = wasHolding;
		return ok;
	}

	void firePending(int i) {
		auto f = m_fields.self();
		auto& pd = f->pending[i];
		if (!pd.active) return;
		pd.active = false;
		GJBaseGameLayer::handleButton(pd.down, 1, i == 0);
	}

	void handleButton(bool down, int button, bool isPlayer1) {
		int i = isPlayer1 ? 0 : 1;
		auto p = isPlayer1 ? m_player1 : m_player2;

		// A newer input arrived: apply any older delayed one first so order is kept
		if (!sim::active) firePending(i);

		bool skip = sim::active || button != 1 || !p ||
		            !Mod::get()->getSettingValue<bool>("enabled");
		if (skip) {
			GJBaseGameLayer::handleButton(down, button, isPlayer1);
			return;
		}

		int maxDelay  = static_cast<int>(Mod::get()->getSettingValue<int64_t>("max-delay"));
		int lookahead = static_cast<int>(Mod::get()->getSettingValue<int64_t>("lookahead"));
		lookahead = std::max(lookahead, 1);

		// If doing it right now is safe, do it right now.
		if (!survives(p, down, 0, lookahead)) {
			// Otherwise see if waiting 1..maxDelay ticks is safe.
			for (int d = 1; d <= maxDelay; d++) {
				if (survives(p, down, d, lookahead)) {
					auto& pd = m_fields->pending[i];
					pd.active = true;
					pd.down = down;
					pd.left = d + 1; // processCommands decrements before applying
					return;          // swallow the input for now
				}
			}
		}
		// Safe now, or nothing helps: click normally.
		GJBaseGameLayer::handleButton(down, button, isPlayer1);
	}

	void processCommands(float dt) {
		sim::dt = dt;
		auto f = m_fields.self();
		PlayerObject* players[2] = {m_player1, m_player2};

		for (int i = 0; i < 2; i++) {
			auto& pd = f->pending[i];
			if (!pd.active) continue;
			if (!players[i] || players[i]->m_isDead) {
				pd.active = false;
				continue;
			}
			if (--pd.left <= 0) firePending(i);
		}
		GJBaseGameLayer::processCommands(dt);
	}
};
