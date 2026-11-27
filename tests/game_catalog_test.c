#include "s31/game_catalog.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    assert(s31_game_platform_from_path("/roms/mario.NES") == S31_GAME_NES);
    assert(s31_game_platform_from_path("/roms/pokemon.gbc") == S31_GAME_GBC);
    assert(s31_game_platform_from_path("/roms/sonic.SMS") == S31_GAME_SMS);
    assert(s31_game_platform_from_path("/roms/columns.gg") == S31_GAME_GG);
    assert(s31_game_platform_from_path("/roms/monaco.sg1000") == S31_GAME_SG1000);
    assert(s31_game_platform_from_path("/roms/bonk.PCE") == S31_GAME_PCE);
    assert(s31_game_platform_from_path("/roms/sf2.zip") == S31_GAME_ARCADE);
    assert(strcmp(s31_game_platform_icon(S31_GAME_NES), "FC") == 0);
    assert(s31_game_catalog_page_move(0, 25, 1) == 10);
    assert(s31_game_catalog_page_move(13, 25, 1) == 20);
    assert(s31_game_catalog_page_move(24, 25, 1) == 0);
    assert(s31_game_catalog_page_move(13, 25, -1) == 0);
    assert(s31_game_catalog_page_move(0, 25, -1) == 20);

    s31_game_nav_repeat_t repeat = {0};
    assert(s31_game_catalog_repeat(&repeat, 1u, 1u, 1000) == 1u);
    assert(s31_game_catalog_repeat(&repeat, 1u, 0, 1399) == 0);
    assert(s31_game_catalog_repeat(&repeat, 1u, 0, 1400) == 1u);
    assert(s31_game_catalog_repeat(&repeat, 1u, 0, 1499) == 0);
    assert(s31_game_catalog_repeat(&repeat, 1u, 0, 1500) == 1u);
    assert(s31_game_catalog_repeat(&repeat, 0, 0, 1510) == 0);
    assert(repeat.held_button == 0);

    uint8_t header[8] = {'N','E','S',0x1a,1,1,0x40,0};
    uint16_t mapper;
    bool supported;
    assert(s31_game_probe_nes(header, sizeof(header), &mapper, &supported) == 0);
    assert(mapper == 4 && supported);
    header[6] = 0xf0; header[7] = 0x10;
    assert(s31_game_probe_nes(header, sizeof(header), &mapper, &supported) == 0);
    assert(mapper == 31 && !supported);

    /* 验证目录缓存保存与读取往返一致性 */
    s31_game_catalog_t saved = {0};
    saved.count = 2;
    strcpy(saved.entries[0].path, "/roms/1942.nes");
    strcpy(saved.entries[0].title, "1942.nes");
    saved.entries[0].platform = S31_GAME_NES;
    saved.entries[0].size = 40976;
    saved.entries[0].mapper = 3;
    saved.entries[0].valid_header = true;
    saved.entries[0].supported = true;
    strcpy(saved.entries[1].path, "/roms/test.gb");
    strcpy(saved.entries[1].title, "test.gb");
    saved.entries[1].platform = S31_GAME_GB;
    saved.entries[1].supported = true;
    assert(s31_game_catalog_save_cache("test_catalog.cache", &saved) == 0);
    s31_game_catalog_t loaded = {0};
    assert(s31_game_catalog_load_cache("test_catalog.cache", &loaded) == 2);
    assert(loaded.count == 2 && strcmp(loaded.entries[0].title, "1942.nes") == 0 &&
           loaded.entries[0].mapper == 3 && loaded.entries[1].platform == S31_GAME_GB);
    remove("test_catalog.cache");
    return 0;
}
