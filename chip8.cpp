#include <SDL2/SDL.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

typedef enum {
  QUIT,
  RUNNING,
  PAUSED
} emulator_state_t;

typedef struct {
  uint16_t opcode;
  uint16_t NNN;
  uint8_t NN;
  uint8_t N;
  uint8_t X;
  uint8_t Y;
} instruction_t;

typedef struct {
  emulator_state_t state;
  uint8_t ram[4096];
  bool display[64*32];
  uint16_t stack[12];
  uint16_t *SP;
  uint8_t V[16];
  uint16_t I;
  uint8_t delay_timer;
  uint8_t sound_timer;
  bool keypad[16];
  const char *rom_name;
  uint16_t PC;
  instruction_t inst;
  bool draw;
} chip8_t;

typedef struct {
  uint32_t window_width;
  uint32_t window_height;
  uint32_t foreground_color;
  uint32_t background_color;
  uint32_t scale_factor;
  bool pixel_outlines;
  uint32_t inst_per_sec;
  uint32_t square_wave_freq;
  uint32_t audio_sample_rate;
  int16_t volume;
} config_t;

typedef struct {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_AudioSpec want, have;
  SDL_AudioDeviceID dev;
} sdl_t;

void audio_callback(void *userdata, uint8_t *stream, int len);

bool init_chip8(chip8_t *chip8, const char rom_name[]) {
  const uint32_t entry_point = 0x200;
  const uint8_t font[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
  };
  memset(chip8, 0, sizeof(chip8_t));
  memcpy(&chip8->ram[0], font, sizeof(font));

  FILE *rom = fopen(rom_name, "rb");
  if (!rom) {
    SDL_Log("Panic: ROM File is invalid !!!\n");
    return false;
  }
  fseek(rom, 0, SEEK_END);
  const size_t rom_size = ftell(rom);
  const size_t max_size = sizeof(chip8->ram) - entry_point;
  rewind(rom);

  if (rom_size > max_size) {
    SDL_Log("Panic: ROM File is Too Large To Handle!!\n");
    fclose(rom);
    return false;
  }
  if (fread(&chip8->ram[entry_point], rom_size, 1, rom) != 1) {
    SDL_Log("Cant Read the ROM \n");
    fclose(rom);
    return false;
  }
  fclose(rom);
  chip8->state = RUNNING;
  chip8->PC = entry_point;
  chip8->rom_name = rom_name;
  chip8->SP = chip8->stack;
  chip8->draw = true;
  return true;
}

bool init_sdl(sdl_t *sdl, config_t *config) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
    SDL_Log("Can't Initialize SDL Subsystem %s \n", SDL_GetError());
    return false;
  }

  sdl->window = SDL_CreateWindow("CHIP-8 Emulator",
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      config->window_width * config->scale_factor,
      config->window_height * config->scale_factor, 0);
  if (!sdl->window) {
    SDL_Log("could not create window %s\n", SDL_GetError());
    return false;
  }

  sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED);
  if (!sdl->renderer) {
    SDL_Log("could not create renderer %s\n", SDL_GetError());
    return false;
  }

  sdl->want = {};
  sdl->want.freq     = 44100;
  sdl->want.format   = AUDIO_S16LSB;
  sdl->want.channels = 1;
  sdl->want.samples  = 512;
  sdl->want.callback = audio_callback;
  sdl->want.userdata = config;
  sdl->dev = SDL_OpenAudioDevice(NULL, 0, &sdl->want, &sdl->have, 0);
  if (sdl->dev == 0) {
    SDL_Log("could not get any audio device %s\n", SDL_GetError());
    return false;
  }
  if ((sdl->want.format != sdl->have.format) ||
      (sdl->want.channels != sdl->have.channels)) {
    SDL_Log("could not get desired audio spec\n");
    return false;
  }
  return true;
}

bool set_config(config_t *config, int argc, char **argv) {
  *config = (config_t){
    .window_width = 64,
    .window_height = 32,
    .foreground_color = 0xFFFFFFFF,
    .background_color = 0x000000FF,
    .scale_factor = 20,
    .pixel_outlines = true,
    .inst_per_sec = 700,
    .square_wave_freq = 440,
    .audio_sample_rate = 44100,
    .volume = 3000,
  };
  for (int i = 0; i < argc; i++) (void)argv[i];
  return true;
} 

void final_cleanup(sdl_t sdl) {
  SDL_DestroyRenderer(sdl.renderer);
  SDL_DestroyWindow(sdl.window);
  SDL_CloseAudioDevice(sdl.dev);
  SDL_Quit();
}

void update_timers(const sdl_t sdl, chip8_t *chip8) {
  if (chip8->delay_timer > 0) chip8->delay_timer--;
  if (chip8->sound_timer > 0) {
    chip8->sound_timer--;
    SDL_PauseAudioDevice(sdl.dev, 0);
  } else {
    SDL_PauseAudioDevice(sdl.dev, 1);
  }
}

void clear_screen(const sdl_t *sdl, const config_t *config) {
  const uint8_t r = (config->background_color >> 24) & 0xFF;
  const uint8_t g = (config->background_color >> 16) & 0xFF;
  const uint8_t b = (config->background_color >> 8)  & 0xFF;
  const uint8_t a = (config->background_color >> 0)  & 0xFF;
  SDL_SetRenderDrawColor(sdl->renderer, r, g, b, a);
  SDL_RenderClear(sdl->renderer);
}

void update_screen(const sdl_t sdl, const config_t config, const chip8_t chip8) {
  SDL_Rect rect = {.x = 0, .y = 0, .w = (int)config.scale_factor, .h = (int)config.scale_factor};

  const uint8_t bg_r = (config.background_color >> 24) & 0xFF;
  const uint8_t bg_g = (config.background_color >> 16) & 0xFF;
  const uint8_t bg_b = (config.background_color >> 8)  & 0xFF;
  const uint8_t bg_a = (config.background_color >> 0)  & 0xFF;
  const uint8_t fg_r = (config.foreground_color >> 24) & 0xFF;
  const uint8_t fg_g = (config.foreground_color >> 16) & 0xFF;
  const uint8_t fg_b = (config.foreground_color >> 8)  & 0xFF;
  const uint8_t fg_a = (config.foreground_color >> 0)  & 0xFF;

  for (uint32_t i = 0; i < sizeof(chip8.display); i++) {
    rect.x = (i % config.window_width) * config.scale_factor;
    rect.y = (i / config.window_width) * config.scale_factor;
    if (chip8.display[i]) {
      SDL_SetRenderDrawColor(sdl.renderer, fg_r, fg_g, fg_b, fg_a);
      SDL_RenderFillRect(sdl.renderer, &rect);
      if (config.pixel_outlines) {
        SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
        SDL_RenderDrawRect(sdl.renderer, &rect);
      }
    } else {
      SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
      SDL_RenderFillRect(sdl.renderer, &rect);
    }
  }
  SDL_RenderPresent(sdl.renderer);
}

void audio_callback(void *userdata, uint8_t *stream, int len) {
  config_t *config = (config_t *)userdata;
  int16_t *audio_data = (int16_t *)stream;
  static uint32_t running_sample_index = 0;
  const int32_t square_wave_period = config->audio_sample_rate / config->square_wave_freq;
  const int32_t half_square_wave_period = square_wave_period / 2;
  for (int i = 0; i < len / 2; i++) {
    audio_data[i] = ((running_sample_index++ / half_square_wave_period) % 2)
                      ? config->volume : -config->volume;
  }
}

void handle_input(chip8_t *chip8) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_QUIT:
        chip8->state = QUIT;
        return;
      case SDL_KEYDOWN:
        switch (event.key.keysym.sym) {
          case SDLK_ESCAPE:   chip8->state = QUIT; return;
          case SDLK_SPACE:
            chip8->state = (chip8->state == RUNNING) ? PAUSED : RUNNING;
            break;
          case SDLK_BACKSPACE: init_chip8(chip8, chip8->rom_name); break;
          case SDLK_1: chip8->keypad[0x1] = true; break;
          case SDLK_2: chip8->keypad[0x2] = true; break;
          case SDLK_3: chip8->keypad[0x3] = true; break;
          case SDLK_4: chip8->keypad[0xC] = true; break;
          case SDLK_q: chip8->keypad[0x4] = true; break;
          case SDLK_w: chip8->keypad[0x5] = true; break;
          case SDLK_e: chip8->keypad[0x6] = true; break;
          case SDLK_r: chip8->keypad[0xD] = true; break;
          case SDLK_a: chip8->keypad[0x7] = true; break;
          case SDLK_s: chip8->keypad[0x8] = true; break;
          case SDLK_d: chip8->keypad[0x9] = true; break;
          case SDLK_f: chip8->keypad[0xE] = true; break;
          case SDLK_z: chip8->keypad[0xA] = true; break;
          case SDLK_x: chip8->keypad[0x0] = true; break;
          case SDLK_c: chip8->keypad[0xB] = true; break;
          case SDLK_v: chip8->keypad[0xF] = true; break;
          default: break;
        }
        break;
      case SDL_KEYUP:
        switch (event.key.keysym.sym) {
          case SDLK_1: chip8->keypad[0x1] = false; break;
          case SDLK_2: chip8->keypad[0x2] = false; break;
          case SDLK_3: chip8->keypad[0x3] = false; break;
          case SDLK_4: chip8->keypad[0xC] = false; break;
          case SDLK_q: chip8->keypad[0x4] = false; break;
          case SDLK_w: chip8->keypad[0x5] = false; break;
          case SDLK_e: chip8->keypad[0x6] = false; break;
          case SDLK_r: chip8->keypad[0xD] = false; break;
          case SDLK_a: chip8->keypad[0x7] = false; break;
          case SDLK_s: chip8->keypad[0x8] = false; break;
          case SDLK_d: chip8->keypad[0x9] = false; break;
          case SDLK_f: chip8->keypad[0xE] = false; break;
          case SDLK_z: chip8->keypad[0xA] = false; break;
          case SDLK_x: chip8->keypad[0x0] = false; break;
          case SDLK_c: chip8->keypad[0xB] = false; break;
          case SDLK_v: chip8->keypad[0xF] = false; break;
          default: break;
        }
        break;
      default: break;
    }
  }
}

void emulate_instructions(chip8_t *chip8, const config_t config) {
  chip8->inst.opcode = (chip8->ram[chip8->PC] << 8) | chip8->ram[chip8->PC + 1];
  chip8->PC += 2;

  chip8->inst.NNN = chip8->inst.opcode & 0x0FFF;
  chip8->inst.NN  = chip8->inst.opcode & 0x00FF;
  chip8->inst.N   = chip8->inst.opcode & 0x000F;
  chip8->inst.X   = (chip8->inst.opcode >> 8) & 0x0F;
  chip8->inst.Y   = (chip8->inst.opcode >> 4) & 0x0F;

  switch ((chip8->inst.opcode >> 12) & 0x0F) {
    case 0x00:
      if (chip8->inst.NN == 0xE0) {
        memset(chip8->display, false, sizeof(chip8->display));
        chip8->draw = true;
      } else if (chip8->inst.NN == 0xEE) {
        chip8->PC = *--chip8->SP;
      }
      break;

    case 0x01: chip8->PC = chip8->inst.NNN; break;

    case 0x02:
      *chip8->SP++ = chip8->PC;
      chip8->PC = chip8->inst.NNN;
      break;

    case 0x03:
      if (chip8->V[chip8->inst.X] == chip8->inst.NN) chip8->PC += 2;
      break;

    case 0x04:
      if (chip8->V[chip8->inst.X] != chip8->inst.NN) chip8->PC += 2;
      break;

    case 0x05:
      if (chip8->inst.N != 0) break;
      if (chip8->V[chip8->inst.X] == chip8->V[chip8->inst.Y]) chip8->PC += 2;
      break;

    case 0x06: chip8->V[chip8->inst.X] = chip8->inst.NN; break;
    case 0x07: chip8->V[chip8->inst.X] += chip8->inst.NN; break;

    case 0x08:
      switch (chip8->inst.N) {
        case 0x0: chip8->V[chip8->inst.X]  = chip8->V[chip8->inst.Y]; break;
        case 0x1: chip8->V[chip8->inst.X] |= chip8->V[chip8->inst.Y]; break;
        case 0x2: chip8->V[chip8->inst.X] &= chip8->V[chip8->inst.Y]; break;
        case 0x3: chip8->V[chip8->inst.X] ^= chip8->V[chip8->inst.Y]; break;
        case 0x4: {
          uint16_t sum = chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y];
          chip8->V[0xF] = (sum > 255) ? 1 : 0;
          chip8->V[chip8->inst.X] = sum & 0xFF;
          break;
        }
        case 0x5: {
          uint8_t vx = chip8->V[chip8->inst.X];
          uint8_t vy = chip8->V[chip8->inst.Y];
          chip8->V[0xF] = (vx >= vy) ? 1 : 0;
          chip8->V[chip8->inst.X] = vx - vy;
          break;
        }
        case 0x6:
          chip8->V[0xF] = chip8->V[chip8->inst.X] & 1;
          chip8->V[chip8->inst.X] >>= 1;
          break;
        case 0x7: {
          uint8_t vx = chip8->V[chip8->inst.X];
          uint8_t vy = chip8->V[chip8->inst.Y];
          chip8->V[0xF] = (vy >= vx) ? 1 : 0;
          chip8->V[chip8->inst.X] = vy - vx;
          break;
        }
        case 0xE:
          chip8->V[0xF] = (chip8->V[chip8->inst.X] & 0x80) >> 7;
          chip8->V[chip8->inst.X] <<= 1;
          break;
        default: break;
      }
      break;

    case 0x09:
      if (chip8->V[chip8->inst.X] != chip8->V[chip8->inst.Y]) chip8->PC += 2;
      break;

    case 0x0A: chip8->I = chip8->inst.NNN; break;
    case 0x0B: chip8->PC = chip8->inst.NNN + chip8->V[0]; break;
    case 0x0C:
      chip8->V[chip8->inst.X] = (rand() % 256) & chip8->inst.NN;
      break;

    case 0x0D: { 
      uint8_t x = chip8->V[chip8->inst.X] % config.window_width;
      uint8_t y = chip8->V[chip8->inst.Y] % config.window_height;
      uint8_t height = chip8->inst.N;
      chip8->V[0xF] = 0;

      for (uint8_t row = 0; row < height; row++) {
        uint8_t py = y + row;
        if (py >= config.window_height) break;
        uint8_t sprite_data = chip8->ram[chip8->I + row];
        for (int col = 7; col >= 0; col--) {
          uint8_t px = x + (7 - col);
          if (px >= config.window_width) break;
          bool *pixel = &chip8->display[py * config.window_width + px];
          bool sprite_bit = (sprite_data & (1 << col)) != 0;
          if (sprite_bit && *pixel) chip8->V[0xF] = 1;
          *pixel ^= sprite_bit;
        }
      }
      chip8->draw = true;
      break;
    }

    case 0x0E:
      switch (chip8->inst.NN) {
        case 0x9E:
          if (chip8->keypad[chip8->V[chip8->inst.X]] == true) chip8->PC += 2;
          break;
        case 0xA1:
          if (chip8->keypad[chip8->V[chip8->inst.X]] == false) chip8->PC += 2;
          break;
        default: break;
      }
      break;

    case 0x0F:
      switch (chip8->inst.NN) {
        case 0x0A: {
          bool key_pressed = false;
          for (uint8_t i = 0; i < 16; i++) {
            if (chip8->keypad[i]) {
              chip8->V[chip8->inst.X] = i;
              key_pressed = true;
              break;
            }
          }
          if (!key_pressed) chip8->PC -= 2;
          break;
        }
        case 0x1E: chip8->I += chip8->V[chip8->inst.X]; break;
        case 0x07: chip8->V[chip8->inst.X] = chip8->delay_timer; break;
        case 0x15: chip8->delay_timer = chip8->V[chip8->inst.X]; break;
        case 0x18: chip8->sound_timer = chip8->V[chip8->inst.X]; break;
        case 0x29:
          chip8->I = (chip8->V[chip8->inst.X] & 0x0F) * 5;
          break;
        case 0x33: {
          uint8_t bcd = chip8->V[chip8->inst.X];
          chip8->ram[chip8->I + 2] = bcd % 10; bcd /= 10;
          chip8->ram[chip8->I + 1] = bcd % 10; bcd /= 10;
          chip8->ram[chip8->I]     = bcd % 10;
          break;
        }
        case 0x55:
          for (uint8_t i = 0; i <= chip8->inst.X; i++)
            chip8->ram[chip8->I + i] = chip8->V[i];
          break;
        case 0x65:
          for (uint8_t i = 0; i <= chip8->inst.X; i++)
            chip8->V[i] = chip8->ram[chip8->I + i];
          break;
        default: break;
      }
      break;

    default: break;
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("NO ROM PASSED\n");
    exit(EXIT_FAILURE);
  }
  config_t config = {};
  if (!set_config(&config, argc, argv)) exit(EXIT_FAILURE);

  sdl_t sdl = {};
  if (!init_sdl(&sdl, &config)) exit(EXIT_FAILURE);

  chip8_t chip8 = {};
  if (!init_chip8(&chip8, argv[1])) exit(EXIT_FAILURE);

  clear_screen(&sdl, &config);

  srand(time(NULL));

  while (chip8.state != QUIT) {
    handle_input(&chip8);
    if (chip8.state == PAUSED) continue;

    uint64_t start = SDL_GetPerformanceCounter();
    for (uint32_t i = 0; i < config.inst_per_sec / 60; i++)
      emulate_instructions(&chip8, config);
    uint64_t end = SDL_GetPerformanceCounter();
    double time_elapsed = (double)((end - start) * 1000) / SDL_GetPerformanceFrequency();
    SDL_Delay((Uint32)(16.67f > time_elapsed ? 16.67f - time_elapsed : 0));

    if (chip8.draw) {
      update_screen(sdl, config, chip8);
      chip8.draw = false;
    }
    update_timers(sdl, &chip8);
  }
  final_cleanup(sdl);
  return EXIT_SUCCESS;
}
