#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <algorithm>
#include <cmath>
#include <random>
#include <string>

using namespace geode::prelude;

static int g_mode = 0;
static bool g_mini = false;
static bool g_flip = false;
static bool g_plat = false;
static constexpr int MODE_COUNT = 8;
static constexpr char const* MODE_NAMES[] = {
	"Cube", "Ship", "Ball", "UFO", "Wave", "Robot", "Spider", "Swing"
};

static void forEachPlayer(auto fn) {
	auto pl = PlayLayer::get();
	if (!pl) return;
	for (auto* p : {pl->m_player1, pl->m_player2}) {
		if (p) fn(p);
	}
}

static void applyMode(int mode) {
	forEachPlayer([mode](PlayerObject* p) {
		p->toggleFlyMode(false, false);
		p->toggleBirdMode(false, false);
		p->toggleRollMode(false, false);
		p->toggleDartMode(false, false);
		p->toggleRobotMode(false, false);
		p->toggleSpiderMode(false, false);
		p->toggleSwingMode(false, false);
		switch (mode) {
			case 1: p->toggleFlyMode(true, false); break;
			case 2: p->toggleRollMode(true, false); break;
			case 3: p->toggleBirdMode(true, false); break;
			case 4: p->toggleDartMode(true, false); break;
			case 5: p->toggleRobotMode(true, false); break;
			case 6: p->toggleSpiderMode(true, false); break;
			case 7: p->toggleSwingMode(true, false); break;
			default: break;
		}
	});
}

$execute {
	listenForSettingChanges<double>("speed", [](double value) {
		if (PlayLayer::get()) {
			CCDirector::get()->getScheduler()->setTimeScale(static_cast<float>(value));
		}
	});
}

class CheatPopup : public geode::Popup {
protected:
	CCLabelBMFont* m_modeLabel = nullptr;
	CCLabelBMFont* m_speedLabel = nullptr;
	CCLabelBMFont* m_powerLabel = nullptr;

	bool init() {
		bool inLevel = PlayLayer::get() != nullptr;
		float h = inLevel ? 330.f : 245.f;
		if (!Popup::init(280.f, h)) return false;
		this->setTitle("Cheats");

		float y = h / 2.f - 62.f;
		auto next = [&y]() {
			float cur = y;
			y -= 27.f;
			return cur;
		};

		auto addToggle = [this](char const* text, bool state, SEL_MenuHandler cb, float ty) {
			auto label = CCLabelBMFont::create(text, "bigFont.fnt");
			label->setScale(0.55f);
			m_mainLayer->addChildAtPosition(label, Anchor::Center, {-50.f, ty});

			auto toggle = CCMenuItemToggler::createWithStandardSprites(this, cb, 0.65f);
			toggle->toggle(state);
			m_buttonMenu->addChildAtPosition(toggle, Anchor::Center, {80.f, ty});
		};

		auto addStepper = [this](CCLabelBMFont*& label, SEL_MenuHandler down, SEL_MenuHandler up, float ty) {
			label = CCLabelBMFont::create("", "bigFont.fnt");
			label->setScale(0.5f);
			m_mainLayer->addChildAtPosition(label, Anchor::Center, {0.f, ty});

			auto minus = ButtonSprite::create("-");
			minus->setScale(0.6f);
			auto minusBtn = CCMenuItemSpriteExtra::create(minus, this, down);
			m_buttonMenu->addChildAtPosition(minusBtn, Anchor::Center, {-100.f, ty});

			auto plus = ButtonSprite::create("+");
			plus->setScale(0.6f);
			auto plusBtn = CCMenuItemSpriteExtra::create(plus, this, up);
			m_buttonMenu->addChildAtPosition(plusBtn, Anchor::Center, {100.f, ty});
		};

		addToggle("Noclip", Mod::get()->getSettingValue<bool>("noclip"), menu_selector(CheatPopup::onNoclip), next());
		addToggle("Fly", Mod::get()->getSettingValue<bool>("fly"), menu_selector(CheatPopup::onFly), next());
		if (inLevel) {
			addToggle("Mini", g_mini, menu_selector(CheatPopup::onMini), next());
			addToggle("Flip gravity", g_flip, menu_selector(CheatPopup::onFlip), next());
			addToggle("Platformer", g_plat, menu_selector(CheatPopup::onPlat), next());
			addStepper(m_modeLabel, menu_selector(CheatPopup::onModeDown), menu_selector(CheatPopup::onModeUp), next());
		}
		addStepper(m_speedLabel, menu_selector(CheatPopup::onSpeedDown), menu_selector(CheatPopup::onSpeedUp), next());
		addStepper(m_powerLabel, menu_selector(CheatPopup::onPowerDown), menu_selector(CheatPopup::onPowerUp), next());

		if (!inLevel) {
			auto hint = CCLabelBMFont::create("Player modes: open from the pause screen", "bigFont.fnt");
			hint->setScale(0.3f);
			m_mainLayer->addChildAtPosition(hint, Anchor::Center, {0.f, y + 8.f});
			y -= 22.f;
		}

		auto allSpr = ButtonSprite::create("All settings");
		allSpr->setScale(0.65f);
		auto allBtn = CCMenuItemSpriteExtra::create(allSpr, this, menu_selector(CheatPopup::onAllSettings));
		m_buttonMenu->addChildAtPosition(allBtn, Anchor::Center, {0.f, y - 6.f});

		this->refresh();
		return true;
	}

	void refresh() {
		auto speed = Mod::get()->getSettingValue<double>("speed");
		auto power = Mod::get()->getSettingValue<double>("fly-power");
		if (m_modeLabel) m_modeLabel->setString(fmt::format("Mode: {}", MODE_NAMES[g_mode]).c_str());
		m_speedLabel->setString(fmt::format("Speed: {:.1f}x", speed).c_str());
		m_powerLabel->setString(fmt::format("Fly power: {:.0f}", power).c_str());
	}

	void changeMode(int delta) {
		g_mode = (g_mode + delta + MODE_COUNT) % MODE_COUNT;
		applyMode(g_mode);
		this->refresh();
	}

	void changeSpeed(double delta) {
		auto v = Mod::get()->getSettingValue<double>("speed") + delta;
		v = std::clamp(std::round(v * 10.0) / 10.0, 0.1, 2.0);
		Mod::get()->setSettingValue<double>("speed", v);
		this->refresh();
	}

	void changePower(double delta) {
		auto v = Mod::get()->getSettingValue<double>("fly-power") + delta;
		v = std::clamp(std::round(v), 3.0, 16.0);
		Mod::get()->setSettingValue<double>("fly-power", v);
		this->refresh();
	}

	void onNoclip(CCObject* sender) {
		auto t = static_cast<CCMenuItemToggler*>(sender);
		Mod::get()->setSettingValue<bool>("noclip", !t->isToggled());
	}

	void onFly(CCObject* sender) {
		auto t = static_cast<CCMenuItemToggler*>(sender);
		Mod::get()->setSettingValue<bool>("fly", !t->isToggled());
	}

	void onMini(CCObject* sender) {
		auto t = static_cast<CCMenuItemToggler*>(sender);
		g_mini = !t->isToggled();
		forEachPlayer([](PlayerObject* p) { p->togglePlayerScale(g_mini, true); });
	}

	void onFlip(CCObject* sender) {
		auto t = static_cast<CCMenuItemToggler*>(sender);
		g_flip = !t->isToggled();
		forEachPlayer([](PlayerObject* p) { p->flipGravity(g_flip, true); });
	}

	void onPlat(CCObject* sender) {
		auto t = static_cast<CCMenuItemToggler*>(sender);
		g_plat = !t->isToggled();
		forEachPlayer([](PlayerObject* p) { p->togglePlatformerMode(g_plat); });
	}

	void onModeDown(CCObject*) { this->changeMode(-1); }
	void onModeUp(CCObject*) { this->changeMode(1); }
	void onSpeedDown(CCObject*) { this->changeSpeed(-0.1); }
	void onSpeedUp(CCObject*) { this->changeSpeed(0.1); }
	void onPowerDown(CCObject*) { this->changePower(-1.0); }
	void onPowerUp(CCObject*) { this->changePower(1.0); }

	void onAllSettings(CCObject*) {
		openSettingsPopup(Mod::get(), false);
	}

public:
	static CheatPopup* create() {
		auto ret = new CheatPopup();
		if (ret->init()) {
			ret->autorelease();
			return ret;
		}
		delete ret;
		return nullptr;
	}
};

// ---------- Main menu ----------
class $modify(CheatMenu, MenuLayer) {
	bool init() {
		if (!MenuLayer::init()) return false;

		auto winSize = CCDirector::get()->getWinSize();
		auto btn = CCMenuItemSpriteExtra::create(
			ButtonSprite::create("Mod menu"), this, menu_selector(CheatMenu::onCheats)
		);
		btn->setScale(0.6f);

		auto menu = CCMenu::create();
		menu->addChild(btn);
		menu->setPosition({70.f, winSize.height - 30.f});
		this->addChild(menu);
		return true;
	}

	void onCheats(CCObject*) {
		if (auto popup = CheatPopup::create()) popup->show();
	}
};

// ---------- Pause screen ----------
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
		if (auto popup = CheatPopup::create()) popup->show();
	}
};

// ---------- Editor pause screen ----------
class $modify(GenEditorPause, EditorPauseLayer) {
	void customSetup() {
		EditorPauseLayer::customSetup();

		auto winSize = CCDirector::get()->getWinSize();

		auto genBtn = CCMenuItemSpriteExtra::create(
			ButtonSprite::create("Generate"), this, menu_selector(GenEditorPause::onGenerate)
		);
		genBtn->setScale(0.7f);
		genBtn->setPosition({0.f, 0.f});

		auto cheatBtn = CCMenuItemSpriteExtra::create(
			ButtonSprite::create("Mod menu"), this, menu_selector(GenEditorPause::onCheats)
		);
		cheatBtn->setScale(0.7f);
		cheatBtn->setPosition({0.f, -35.f});

		auto menu = CCMenu::create();
		menu->addChild(genBtn);
		menu->addChild(cheatBtn);
		menu->setPosition({70.f, winSize.height - 40.f});
		this->addChild(menu);
	}

	void onCheats(CCObject*) {
		if (auto popup = CheatPopup::create()) popup->show();
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

// ---------- Cheat logic ----------
class $modify(CheatLayer, PlayLayer) {
	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

		g_mode = 0;
		g_mini = false;
		g_flip = false;
		g_plat = false;
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
