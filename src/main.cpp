#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <random>

using namespace geode::prelude;

static constexpr float SPEED_MIN = 0.1f;
static constexpr float SPEED_MAX = 2.0f;

// ---------- Menu popup (touch friendly) ----------
class CheatPopup : public geode::Popup<> {
protected:
	bool setup() override {
		this->setTitle("Cheats");

		auto addToggle = [this](char const* text, char const* key, SEL_MenuHandler cb, float y) {
			auto label = CCLabelBMFont::create(text, "bigFont.fnt");
			label->setScale(0.6f);
			m_mainLayer->addChildAtPosition(label, Anchor::Center, {-45.f, y});

			auto toggle = CCMenuItemToggler::createWithStandardSprites(this, cb, 0.8f);
			toggle->toggle(Mod::get()->getSettingValue<bool>(key));
			m_buttonMenu->addChildAtPosition(toggle, Anchor::Center, {80.f, y});
		};

		addToggle("Noclip", "noclip", menu_selector(CheatPopup::onNoclip), 40.f);
		addToggle("Fly", "fly", menu_selector(CheatPopup::onFly), 5.f);

		m_speedLabel = CCLabelBMFont::create("", "bigFont.fnt");
		m_speedLabel->setScale(0.5f);
		m_mainLayer->addChildAtPosition(m_speedLabel, Anchor::Center, {0.f, -30.f});

		auto speed = static_cast<float>(Mod::get()->getSettingValue<double>("speed"));
		m_slider = Slider::create(this, menu_selector(CheatPopup::onSpeed), 0.8f);
		m_slider->setValue((speed - SPEED_MIN) / (SPEED_MAX - SPEED_MIN));
		m_buttonMenu->addChildAtPosition(m_slider, Anchor::Center, {0.f, -55.f});
		this->updateSpeedLabel(speed);

		return true;
	}

	CCLabelBMFont* m_speedLabel = nullptr;
	Slider* m_slider = nullptr;

	void updateSpeedLabel(float speed) {
		m_speedLabel->setString(fmt::format("Speed: {:.2f}x", speed).c_str());
	}

	void onNoclip(CCObject* sender) {
		auto t = static_cast<CCMenuItemToggler*>(sender);
		Mod::get()->setSettingValue<bool>("noclip", !t->isToggled());
	}

	void onFly(CCObject* sender) {
		auto t = static_cast<CCMenuItemToggler*>(sender);
		Mod::get()->setSettingValue<bool>("fly", !t->isToggled());
	}

	void onSpeed(CCObject* sender) {
		float v = m_slider->getThumb()->getValue();
		float speed = SPEED_MIN + v * (SPEED_MAX - SPEED_MIN);
		Mod::get()->setSettingValue<double>("speed", speed);
		CCDirector::get()->getScheduler()->setTimeScale(speed);
		this->updateSpeedLabel(speed);
	}

public:
	static CheatPopup* create() {
		auto ret = new CheatPopup();
		if (ret->initAnchored(260.f, 200.f)) {
			ret->autorelease();
			return ret;
		}
		delete ret;
		return nullptr;
	}
};

// ---------- Button in the pause menu ----------
class $modify(CheatPause, PauseLayer) {
	void customSetup() {
		PauseLayer::customSetup();

		auto winSize = CCDirector::get()->getWinSize();
		CCNode* icon = CCSprite::create("menu-icon.png"_spr);
		if (icon) icon->setScale(0.35f);
		else icon = ButtonSprite::create("Cheats");

		auto btn = CCMenuItemSpriteExtra::create(
			icon, this, menu_selector(CheatPause::onCheats)
		);

		auto menu = CCMenu::create();
		menu->addChild(btn);
		menu->setPosition({winSize.width - 60.f, 30.f});
		this->addChild(menu);
	}

	void onCheats(CCObject*) {
		if (auto popup = CheatPopup::create()) popup->show();
	}
};

// ---------- Level generator (editor pause menu) ----------
class $modify(GenEditorPause, EditorPauseLayer) {
	void customSetup() {
		EditorPauseLayer::customSetup();

		auto winSize = CCDirector::get()->getWinSize();
		CCNode* icon = CCSprite::create("menu-icon.png"_spr);
		if (icon) icon->setScale(0.35f);
		else icon = ButtonSprite::create("Generate");

		auto btn = CCMenuItemSpriteExtra::create(
			icon, this, menu_selector(GenEditorPause::onGenerate)
		);
		auto menu = CCMenu::create();
		menu->addChild(btn);
		menu->setPosition({60.f, winSize.height - 40.f});
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

// ---------- Cheat logic ----------
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

		auto power = static_cast<float>(Mod::get()->getSettingValue
