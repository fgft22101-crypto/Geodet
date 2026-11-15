#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <random>

using namespace geode::prelude;

$execute {
	listenForSettingChanges<double>("speed", [](double value) {
		if (PlayLayer::get()) {
			CCDirector::get()->getScheduler()->setTimeScale(static_cast<float>(value));
		}
	});
}

class $modify(CheatPause, PauseLayer) {
	void customSetup() {
		PauseLayer::customSetup();

		auto winSize = CCDirector::get()->getWinSize();
		auto btn = CCMenuItemSpriteExtra::create(
			ButtonSprite::create("Cheats"), this, menu_selector(CheatPause::onCheats)
		);
		btn->setScale(0.8f);

		auto menu = CCMenu::create();
		menu->addChild(btn);
		menu->setPosition({winSize.width / 2.f, 20.f});
		this->addChild(menu);
	}

	void onCheats(CCObject*) {
		openSettingsPopup(Mod::get(), false);
	}
};

class $modify(GenEditorPause, EditorPauseLayer) {
	void customSetup() {
		EditorPauseLayer::customSetup();

		auto winSize = CCDirector::get()->getWinSize();
		auto btn = CCMenuItemSpriteExtra::create(
			ButtonSprite::create("Generate"), this, menu_selector(GenEditorPause::onGenerate)
		);
		btn->setScale(0.7f);

		auto menu = CCMenu::create();
		menu->addChild(btn);
		menu->setPosition({70.f, winSize.height - 40.f});
		this->addChild(menu);
	}

	void onGenerate(CCObject*) {
		constexpr float CELL = 30.f;
		constexpr float GROUND = 105.f;
		constexpr int BLOCK = 1;
		constexpr int SPIKE = 8;

		auto length = static_cast<int>(Mod::get()->getSettingValue<int64_t>("gen-length"));
		std::mt19937 rng{std::random_device{}()};
		auto rnd = [&](int a, int b) {
			return std::uniform_int_distribution<int>(a, b)(rng);
		};

		int count = 0;
		auto put = [&](int id, int cellX, int row) {
			float x = cellX * CELL + CELL / 2.f;
			float y = GROUND + row * CELL;
			m_editorLayer->createObject(id, CCPoint{x, y}, true);
			count++;
		};

		int x = 12;
		while (x < length) {
			switch (rnd(0, 4)) {
				case 0:
					put(SPIKE, x, 0);
					x += 1;
					break;
				case 1:
					put(SPIKE, x, 0);
					put(SPIKE, x + 1, 0);
					x += 2;
					break;
				case 2:
					for (int i = 0; i < 3; i++) put(BLOCK, x + i, 0);
					x += 3;
					break;
				case 3:
					for (int i = 0; i < 3; i++) put(BLOCK, x + i, 0);
					put(SPIKE, x + 5, 0);
					x += 6;
					break;
				default:
					put(BLOCK, x, 0);
					put(BLOCK, x + 1, 0);
					put(BLOCK, x + 1, 1);
					put(SPIKE, x + 4, 0);
					x += 5;
					break;
			}
			x += rnd(5, 8);
		}

		Notification::create(
			fmt::format("Generated {} objects", count),
			NotificationIcon::Success
		)->show();
	}
};

class $modify(CheatLayer, PlayLayer) {
	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

		auto speed = static_cast<float>(Mod::get()->getSettingValue<double>("speed"));
		CCDirector::get()->getScheduler()->setTimeScale(speed);
		return true;
	}

	void destroyPlayer(PlayerObject* player, GameObject* object) {
		if (Mod::get()->getSettingValue<bool>("noclip") && object != m_anticheatSpike) {
			return;
		}
		PlayLayer::destroyPlayer(player, object);
	}

	void onQuit() {
		CCDirector::get()->getScheduler()->setTimeScale(1.f);
		PlayLayer::onQuit();
	}
};

class $modify(FlyPlayer, PlayerObject) {
	struct Fields {
		bool held = false;
	};

	bool pushButton(PlayerButton button) {
		if (button == PlayerButton::Jump) m_fields->held = true;
		return PlayerObject::pushButton(button);
	}

	bool releaseButton(PlayerButton button) {
		if (button == PlayerButton::Jump) m_fields->held = false;
		return PlayerObject::releaseButton(button);
	}

	void update(float dt) {
		PlayerObject::update(dt);

		if (!m_fields->held) return;
		if (!Mod::get()->getSettingValue<bool>("fly")) return;
		if (m_isShip || m_isBird || m_isDart || m_isSwing) return;

		auto power = static_cast<float>(Mod::get()->getSettingValue<double>("fly-power"));
		m_yVelocity = m_isUpsideDown ? -power : power;
	}
};
