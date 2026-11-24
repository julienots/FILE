#include <doctest/doctest.h>

#include "agr/runtime/Localization.hpp"
#include "agr/ui/UIComponents.hpp"
#include "agr/ui/UISystem.hpp"

using namespace agr;

TEST_SUITE("localization") {

TEST_CASE("CSV: header, quotes, escaped quotes, line breaks, BOM, semicolons") {
    Localization l;
    REQUIRE(l.parse("\xEF\xBB\xBF"
                    "key,fr,en\r\n"
                    "menu.play,Jouer,Play\n"
                    "hud.score,\"Score : {1}\",\"Score: {1}\"\n"
                    "quote,\"Il a dit \"\"bonjour\"\"\",\"He said \"\"hi\"\"\"\n"
                    "multi,\"ligne 1\nligne 2\",\n"
                    "\n"));
    CHECK(l.languages() == std::vector<std::string>{"fr", "en"});
    CHECK(l.keys().size() == 4);
    CHECK(l.language() == "fr");  // first column by default
    CHECK(l.tr("menu.play") == "Jouer");
    CHECK(l.tr("quote") == "Il a dit \"bonjour\"");
    CHECK(l.tr("multi") == "ligne 1\nligne 2");
    REQUIRE(l.setLanguage("en"));
    CHECK(l.tr("quote") == "He said \"hi\"");
    CHECK(l.tr("multi") == "ligne 1\nligne 2");  // empty English cell: first language
    CHECK(l.tr("missing.key") == "missing.key");
    CHECK(l.format("hud.score", {"42"}) == "Score: 42");

    Localization semi;
    REQUIRE(semi.parse("key;fr;en\nok;Oui;Yes\n"));
    CHECK(semi.tr("ok") == "Oui");

    // Round trip through toCsv.
    Localization back;
    REQUIRE(back.parse(l.toCsv()));
    CHECK(back.keys() == l.keys());
    for (usize r = 0; r < l.keys().size(); ++r)
        for (usize c = 0; c < l.languages().size(); ++c) CHECK(back.cell(r, c) == l.cell(r, c));
}

TEST_CASE("language choice: exact, base language, system order, fallback") {
    Localization l;
    REQUIRE(l.parse("key,en,fr,pt-BR\nhi,Hello,Bonjour,Olá\n"));
    CHECK(l.setLanguage("FR-ca"));  // base language, case-insensitive
    CHECK(l.language() == "fr");
    CHECK(l.setLanguage("pt-BR"));
    CHECK(l.tr("hi") == "Olá");
    CHECK_FALSE(l.setLanguage("de"));
    CHECK(l.language() == "pt-BR");  // unchanged
    l.chooseLanguage({"de-DE", "fr-FR", "en"});
    CHECK(l.language() == "fr");
    l.chooseLanguage({"ja"});
    CHECK(l.language() == "en");  // nothing matches: first column
    const u64 before = l.revision();
    l.setLanguage("fr");
    CHECK(l.revision() > before);
}

TEST_CASE("errors and editing") {
    Localization l;
    CHECK_FALSE(l.parse("key,fr\na,\"unterminated\n"));
    CHECK_FALSE(l.parse("key,fr\na,1\na,2\n"));  // duplicate key
    CHECK_FALSE(l.parse("key\n"));                // no language
    REQUIRE(l.parse(""));
    CHECK(l.languages().empty());
    CHECK(l.tr("x") == "x");
    CHECK(l.addLanguage("fr"));
    CHECK(l.addLanguage("en"));
    CHECK_FALSE(l.addLanguage("fr"));
    CHECK(l.addKey("title"));
    CHECK_FALSE(l.addKey("title"));
    l.setCell(0, 0, "Titre");
    l.setCell(0, 1, "Title");
    CHECK(l.tr("title") == "Titre");
    CHECK(l.renameKey(0, "menu.title"));
    CHECK(l.tr("menu.title") == "Titre");
    l.removeLanguage(0);
    CHECK(l.language() == "en");
    CHECK(l.tr("menu.title") == "Title");
    l.removeKey(0);
    CHECK(l.keys().empty());
}

TEST_CASE("UI texts with a key show the translation and follow the language") {
    Scene scene;
    registerUIComponents();
    VirtualFileSystem vfs;
    UISystem ui(vfs);
    Localization l;
    REQUIRE(l.parse("key,fr,en\nmenu.play,Jouer,Play\n"));
    ui.setLocalization(&l);
    const auto canvas = scene.createEntity("Canvas");
    scene.registry().emplace<UICanvasComponent>(canvas);
    const auto label = scene.createEntity("Label", canvas);
    scene.registry().emplace<UIRectComponent>(label);
    auto& text = scene.registry().emplace<UITextComponent>(label);
    text.localizationKey = "menu.play";
    const auto plain = scene.createEntity("Plain", canvas);
    scene.registry().emplace<UIRectComponent>(plain);
    scene.registry().emplace<UITextComponent>(plain).text = "Score";
    scene.updateTransforms();
    ui.layout(scene, 800, 600);
    CHECK(scene.registry().get<UITextComponent>(label).text == "Jouer");
    l.setLanguage("en");
    ui.layout(scene, 800, 600);
    CHECK(scene.registry().get<UITextComponent>(label).text == "Play");
    CHECK(scene.registry().get<UITextComponent>(plain).text == "Score");  // no key: untouched
}

}

TEST_CASE("localization plurals: CLDR categories, zero row, fallbacks, agr.trn-style {1}") {
    CHECK(std::string(pluralCategory("en", 1)) == "one");
    CHECK(std::string(pluralCategory("en", 0)) == "other");
    CHECK(std::string(pluralCategory("en-US", 2)) == "other");
    CHECK(std::string(pluralCategory("fr", 0)) == "one");
    CHECK(std::string(pluralCategory("fr", 1.5)) == "one");
    CHECK(std::string(pluralCategory("fr", 2)) == "other");
    for (int n : {1, 21, 101}) CHECK(std::string(pluralCategory("ru", n)) == "one");
    for (int n : {2, 3, 4, 22, 104}) CHECK(std::string(pluralCategory("ru", n)) == "few");
    for (int n : {0, 5, 11, 12, 14, 25, 111}) CHECK(std::string(pluralCategory("ru", n)) == "many");
    CHECK(std::string(pluralCategory("pl", 22)) == "few");
    CHECK(std::string(pluralCategory("pl", 21)) == "many");
    CHECK(std::string(pluralCategory("ar", 2)) == "two");
    CHECK(std::string(pluralCategory("ar", 105)) == "few");
    CHECK(std::string(pluralCategory("ja", 1)) == "other");

    Localization loc;
    REQUIRE(loc.parse("key,en,fr,ru\n"
                      "coins.zero,No coins,,\n"
                      "coins.one,{1} coin,{1} pièce,{1} монета\n"
                      "coins.few,,,{1} монеты\n"
                      "coins.many,,,{1} монет\n"
                      "coins.other,{1} coins,{1} pièces,{1} монеты\n"
                      "lives,{1} lives left for {2},,\n"));
    REQUIRE(loc.setLanguage("en"));
    CHECK(loc.plural("coins", 0) == "No coins");
    CHECK(loc.plural("coins", 1) == "1 coin");
    CHECK(loc.plural("coins", 7) == "7 coins");
    CHECK(loc.plural("coins", 2.5) == "2.5 coins");
    CHECK(loc.plural("lives", 3, {"Ada"}) == "3 lives left for Ada");  // no forms: the key itself
    REQUIRE(loc.setLanguage("fr"));
    CHECK(loc.plural("coins", 0) == "0 pièce");   // .zero empty in French: the rule (0 is "one")
    CHECK(loc.plural("coins", 3) == "3 pièces");
    REQUIRE(loc.setLanguage("ru"));
    CHECK(loc.plural("coins", 21) == "21 монета");
    CHECK(loc.plural("coins", 3) == "3 монеты");
    CHECK(loc.plural("coins", 5) == "5 монет");
}
