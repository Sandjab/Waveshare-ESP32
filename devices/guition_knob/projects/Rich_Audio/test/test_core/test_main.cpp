#include <unity.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "sounds.h"

static int16_t buf[AUDIO_FRAMES_PER_BUF * 2];

// Rend des blocs jusqu'à ce que render() retourne false, et renvoie le nombre de
// blocs rendus (ou -1 si le son n'a pas terminé après `max` blocs).
static int render_until_done(Sound* s, int max) {
    for (int n = 1; n <= max; n++) {
        if (!s->render(buf, AUDIO_FRAMES_PER_BUF)) return n;
    }
    return -1;
}

static bool block_has_signal(const int16_t* b) {
    for (size_t i = 0; i < AUDIO_FRAMES_PER_BUF * 2; i++) {
        if (abs(b[i]) > 100) return true;
    }
    return false;
}

// Le registre commence par les deux sons demandés à l'origine (l'UI liste ça dans
// l'ordre) et a été enrichi d'une vingtaine de presets système.
void test_registry_layout(void) {
    TEST_ASSERT_EQUAL_STRING("Harp", sounds_name(0));
    TEST_ASSERT_EQUAL_STRING("Ping", sounds_name(1));
    TEST_ASSERT_GREATER_OR_EQUAL_INT(22, sounds_count());   // Harp + Ping + ~20
    TEST_ASSERT_NULL(sounds_get(-1));
    TEST_ASSERT_NULL(sounds_get(sounds_count()));
}

// Chaque son du registre a un nom non vide et unique (l'UI les distingue).
void test_all_names_present_and_unique(void) {
    for (int i = 0; i < sounds_count(); i++) {
        const char* a = sounds_name(i);
        TEST_ASSERT_NOT_NULL(a);
        TEST_ASSERT_TRUE(a[0] != '\0');
        for (int j = i + 1; j < sounds_count(); j++) {
            TEST_ASSERT_FALSE(strcmp(a, sounds_name(j)) == 0);
        }
    }
}

// Invariant clé du moteur : TOUT son est one-shot -> render() finit par retourner
// false (la tâche audio coupe l'ampli sur ce false ; un son sans fin laisserait
// l'ampli allumé en permanence). Et après trigger, le son produit réellement du
// signal (sinon « jouer » ne donnerait rien d'audible).
void test_every_sound_is_one_shot_and_audible(void) {
    char msg[64];
    for (int i = 0; i < sounds_count(); i++) {
        Sound* s = sounds_get(i);
        snprintf(msg, sizeof(msg), "[%d] %s", i, sounds_name(i));

        s->trigger();
        s->render(buf, AUDIO_FRAMES_PER_BUF);
        TEST_ASSERT_TRUE_MESSAGE(block_has_signal(buf), msg);   // 1er bloc audible

        // termine dans une borne large (le plus long son < ~1.5 s ~ 260 blocs)
        TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, render_until_done(s, 4000), msg);
    }
}

// Les catégories pavent EXACTEMENT le registre plat : tranches contiguës, sans trou
// ni recouvrement, couvrant [0, sounds_count). L'UI s'appuie là-dessus pour mapper
// chaque item de page vers un index global -> une dérive d'offset jouerait le mauvais son.
void test_categories_tile_registry(void) {
    TEST_ASSERT_GREATER_OR_EQUAL_INT(2, categories_count());
    int expected_start = 0;
    for (int i = 0; i < categories_count(); i++) {
        const SoundCategory* c = category_get(i);
        TEST_ASSERT_NOT_NULL(c);
        TEST_ASSERT_TRUE(c->name && c->name[0] != '\0');
        TEST_ASSERT_EQUAL_INT(expected_start, c->start);   // contiguë avec la précédente
        TEST_ASSERT_GREATER_THAN_INT(0, c->count);
        expected_start += c->count;
    }
    TEST_ASSERT_EQUAL_INT(sounds_count(), expected_start);  // couvre tout, sans reste
    TEST_ASSERT_NULL(category_get(-1));
    TEST_ASSERT_NULL(category_get(categories_count()));
}

// Re-trigger après extinction : le son rejoue (taper deux fois le même item).
void test_retrigger_replays(void) {
    Sound* s = sounds_get(1);   // Ping
    s->trigger();
    TEST_ASSERT_NOT_EQUAL(-1, render_until_done(s, 4000));
    TEST_ASSERT_FALSE(s->render(buf, AUDIO_FRAMES_PER_BUF));   // bien terminé
    s->trigger();                                             // rejoue
    TEST_ASSERT_TRUE(s->render(buf, AUDIO_FRAMES_PER_BUF));    // de nouveau actif
    TEST_ASSERT_TRUE(block_has_signal(buf));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_registry_layout);
    RUN_TEST(test_categories_tile_registry);
    RUN_TEST(test_all_names_present_and_unique);
    RUN_TEST(test_every_sound_is_one_shot_and_audible);
    RUN_TEST(test_retrigger_replays);
    return UNITY_END();
}
