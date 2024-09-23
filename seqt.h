#ifndef SEQT_H
#define SEQT_H

#include <stddef.h>
#include <stdint.h>
#include "riv.h"

#ifndef SEQT_API
#define SEQT_API
#endif

////////////////////////////////////////////////////////////////////////////////
// SEQT constants

enum {
  SEQT_TIME_SIG = 4,
  SEQT_SYNTH_WAVES = 2,
  SEQT_NOTES_TRACKS = 4,
  SEQT_NOTES_PAGES = 7,
  SEQT_NOTES_COLUMNS = 16,
  SEQT_NOTES_ROWS = 10,
  SEQT_NOTES_SCALE_NOTES = 2*SEQT_NOTES_ROWS,
  SEQT_NOTES_TOTAL_COLUMNS = SEQT_NOTES_PAGES*SEQT_NOTES_COLUMNS,
  SEQT_MAX_SOUNDS = 32,
};

////////////////////////////////////////////////////////////////////////////////
// SEQT structures

typedef struct seqt_note {
  int8_t periods;
  int8_t volume;
  int8_t slide;
} seqt_note;

typedef struct seqt_source {
  uint8_t magic[4];
  uint8_t version;
  uint8_t flags;
  int16_t bpm;
  uint32_t track_sizes[SEQT_NOTES_TRACKS];
  seqt_note pages[SEQT_NOTES_TRACKS][SEQT_NOTES_ROWS][SEQT_NOTES_TOTAL_COLUMNS];
} seqt_source;

typedef struct seqt_synth {
  riv_waveform_desc waves[SEQT_SYNTH_WAVES];
} seqt_synth;

typedef struct seqt_soundfont {
  seqt_synth synths[SEQT_NOTES_TRACKS][SEQT_NOTES_ROWS];
  float scale[SEQT_NOTES_SCALE_NOTES];
} seqt_soundfont;

typedef struct seqt_synthnote {
  seqt_synth synth;
  float start_freq;
  float end_freq;
  float amplitude;
  float periods;
  float bps;
} seqt_synthnote;

typedef struct seqt_sound {
  uint64_t id;
  seqt_soundfont *font;
  seqt_source *source;
  uint64_t frame;
  uint64_t stop_frame;
  double speed;
  float pitch;
  float volume;
  uint64_t last_note_frame;
  bool paused;
} seqt_sound;

typedef struct seqt_context {
  seqt_soundfont default_font;
  seqt_sound sounds[SEQT_MAX_SOUNDS+1];
  uint64_t sound_gen_counter;
} seqt_context;

////////////////////////////////////////////////////////////////////////////////
// SEQT API

// Global SEQT context
SEQT_API seqt_context seqt;

// Initialize SEQT context, must be called on initialization.
SEQT_API void seqt_init();
// Poll all sounds, must be called once every frame.
SEQT_API void seqt_poll();

////////////////////////////////////////
// Sound sources

// Load sound source from a file SEQT file
SEQT_API seqt_source *seqt_make_source_from_file(const char *filename);
// Destroy a sound source
SEQT_API void seqt_destroy_source(seqt_source *source);
// Get sound source time length in seconds
SEQT_API double seqt_get_source_length(seqt_source *source);

////////////////////////////////////////
// Sounds

// Play a sound from a source for loop times and returns its it (negative loops plays forever)
SEQT_API uint64_t seqt_play(seqt_source *source, int32_t loops);
// Stop a sound after a delay (in seconds)
SEQT_API void seqt_stop(uint64_t sound_id, double secs);
// Seek a sound to a timestamp (in seconds)
SEQT_API void seqt_seek(uint64_t sound_id, double secs);
// Set sound paused
SEQT_API void seqt_set_paused(uint64_t sound_id, bool paused);
// Set sound speed
SEQT_API void seqt_set_speed(uint64_t sound_id, float speed);
// Set sound pitch
SEQT_API void seqt_set_pitch(uint64_t sound_id, float pitch);
// Set sound volume, values larger than 1 will play louder
SEQT_API void seqt_set_volume(uint64_t sound_id, float volume);

////////////////////////////////////////
// Low level API (avoid using)

// Get a sound structure from its id, may return NULL in case it stopped.
SEQT_API seqt_sound *seqt_get_sound(uint64_t sound_id);

// Play a sound note (used internally)
SEQT_API void seqt_play_note(seqt_synthnote *note);

#endif // SEQT_H

////////////////////////////////////////////////////////////////////////////////
// Implementation

#ifdef SEQT_IMPL

#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

static inline uint64_t maxu(uint64_t a, uint64_t b) { return (a >= b) ? a : b; }
static inline uint64_t minu(uint64_t a, uint64_t b) { return (a <= b) ? a : b; }
static inline uint64_t clampu(uint64_t a, uint64_t min, uint64_t max) { return minu(maxu(a, min), max); }

seqt_context seqt;

void seqt_play_note(seqt_synthnote *note) {
  for (uint64_t i=0;i<SEQT_SYNTH_WAVES;++i) {
    riv_waveform_desc wave = note->synth.waves[i];
    if (wave.type <= 0.0f || note->periods <= 0.0f) {
      continue;
    }
    if (wave.start_frequency <= 8.0f && note->start_freq > 0.0f) {
      wave.start_frequency = wave.start_frequency * note->start_freq;
    } else if (note->start_freq > 0.0f && note->end_freq > 0.0f) {
      wave.start_frequency = wave.start_frequency * (note->start_freq/note->end_freq);
    }
    if (wave.end_frequency <= 8.0f && note->end_freq > 0.0f) {
      wave.end_frequency = wave.end_frequency * note->end_freq;
    }
    wave.attack = note->periods * wave.attack / note->bps;
    wave.decay = note->periods * wave.decay / note->bps;
    wave.sustain = note->periods * wave.sustain / note->bps;
    wave.release = note->periods * wave.release / note->bps;
    wave.amplitude = note->amplitude * wave.amplitude;
    riv_waveform(&wave);
  }
}

static void seqt_poll_sound(seqt_sound *sound) {
  if (sound->paused) {
    return;
  }
  seqt_source *source = sound->source;
  double hits_per_second = (source->bpm * SEQT_TIME_SIG)/60.0;
  uint64_t frame = sound->frame + 1;
  if (frame >= sound->stop_frame) {
    *sound = (seqt_sound){0};
    return;
  }
  sound->frame = frame;
  uint64_t note_frame = floor((frame * hits_per_second * sound->speed) / riv->target_fps);
  // TODO: allow setting loop ranges
  if (note_frame == sound->last_note_frame) {
    return;
  }
  sound->last_note_frame = note_frame;
  seqt_soundfont *font = sound->font;
  for (uint64_t note_z=0;note_z<SEQT_NOTES_TRACKS;++note_z) {
    uint64_t note_x = note_frame % maxu(source->track_sizes[note_z], SEQT_NOTES_COLUMNS);
    // TODO: allow muting tracks
    for (uint64_t note_y=0;note_y<SEQT_NOTES_ROWS;++note_y) {
      seqt_note note = source->pages[note_z][note_y][note_x];
      if (note.periods == 0) {
        continue;
      }
      seqt_synthnote synth_note = {
        .synth = font->synths[note_z][note_y],
        .start_freq = font->scale[note_y + 5],
        .end_freq = font->scale[note_y + 5],
        .amplitude = powf(2.0f, (note.volume/3.0f)) * sound->volume,
        .periods = note.periods,
        .bps = hits_per_second * sound->pitch,
      };
      if (note.slide != 0) {
        synth_note.start_freq = font->scale[clampu(note_y + 5 + note.slide, 0, SEQT_NOTES_SCALE_NOTES-1)];
      }
      seqt_play_note(&synth_note);
    }
  }
}

void seqt_poll() {
  for (uint64_t i=1;i<=SEQT_MAX_SOUNDS;++i) {
    seqt_sound *sound = &seqt.sounds[i];
    if (sound->id != 0) {
      seqt_poll_sound(sound);
    }
  }
}

seqt_source *seqt_make_source_from_file(const char *filename) {
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    riv_printf("failed to open seqt source '%s'\n", filename);
    return NULL;
  }
  seqt_source *source = (seqt_source*)mmap(NULL, sizeof(seqt_source), PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (source == MAP_FAILED) {
    riv_printf("failed to map seqt source '%s'\n", filename);
    close(fd);
    return NULL;
  }
  close(fd);
  if (source->magic[0] != 'S' || source->magic[1] != 'E' || source->magic[2] != 'Q' || source->magic[3] != 'T') {
    riv_printf("malformed seqt source '%s'\n", filename);
    munmap(source, sizeof(seqt_source));
    return NULL;
  }
  return source;
}

void seqt_destroy_source(seqt_source *source) {
  if (source) {
    munmap(source, sizeof(seqt_source));
  }
}

double seqt_get_source_length(seqt_source *source) {
  if (!source) {
    return 0;
  }
  uint64_t track_size = 0;
  for (uint64_t i = 0; i < SEQT_NOTES_TRACKS; ++i) {
    track_size = maxu(source->track_sizes[i], track_size);
  }
  return (track_size * 3600.0) / (source->bpm * SEQT_TIME_SIG);
}

uint64_t seqt_play(seqt_source *source, int32_t loops) {
  if (!source) {
    riv_printf("failed to play seqt sound: invalid seqt source\n");
    return 0;
  }
  for (uint64_t i=1;i<=SEQT_MAX_SOUNDS;++i) {
    seqt_sound *sound = &seqt.sounds[i];
    if (sound->id == 0) {
      uint64_t id = (seqt.sound_gen_counter++ << 32) | i;
      *sound = (seqt_sound){
        .id = id,
        .font = &seqt.default_font,
        .source = source,
        .frame = 0,
        .stop_frame = loops < 0 ? (uint64_t)-1 : loops*seqt_get_source_length(source),
        .speed = 1.0,
        .pitch = 1.0f,
        .volume = 1.0f,
        .last_note_frame = (uint64_t)-1,
        .paused = false,
      };
      return id;
    }
  }
  riv_printf("failed to play seqt sound: too many sounds\n");
  return 0;
}

void seqt_stop(uint64_t sound_id, double secs) {
  seqt_sound *sound = seqt_get_sound(sound_id);
  sound->stop_frame = sound->frame + (uint64_t)(fmax(secs, 0.0) * riv->target_fps);
}

void seqt_seek(uint64_t sound_id, double secs) {
  seqt_get_sound(sound_id)->frame = fmax(secs, 0.0) * riv->target_fps;
}

void seqt_set_paused(uint64_t sound_id, bool paused) {
  seqt_get_sound(sound_id)->paused = paused;
}

void seqt_set_speed(uint64_t sound_id, float speed) {
  seqt_get_sound(sound_id)->speed = speed;
}

void seqt_set_pitch(uint64_t sound_id, float pitch) {
  seqt_get_sound(sound_id)->pitch = pitch;
}

void seqt_set_volume(uint64_t sound_id, float volume) {
  seqt_get_sound(sound_id)->volume = volume;
}

seqt_sound *seqt_get_sound(uint64_t sound_id) {
  seqt_sound *sound = &seqt.sounds[sound_id & (SEQT_MAX_SOUNDS-1)];
  return (sound->id == sound_id) ? sound : NULL;
}

static void seqt_fill_major_pentatonic_scale(float scale[SEQT_NOTES_SCALE_NOTES], int semitone_index) {
  double freq = 110.0 * pow(2.0, ((semitone_index - 45)/12.0));
  for (int i=0; i < 4; ++i) {
    scale[i*5+0] = floor(freq * pow(2.0, ((3-i) + 9.0/12.0)));
    scale[i*5+1] = floor(freq * pow(2.0, ((3-i) + 7.0/12.0)));
    scale[i*5+2] = floor(freq * pow(2.0, ((3-i) + 4.0/12.0)));
    scale[i*5+3] = floor(freq * pow(2.0, ((3-i) + 2.0/12.0)));
    scale[i*5+4] = floor(freq * pow(2.0, ((3-i) + 0.0/12.0)));
  }
}

void seqt_init() {
  // Instruments
  seqt_synth STRINGS_WAVE = {.waves = {{
    .type = RIV_WAVEFORM_TRIANGLE,
    .attack = 0.1f, .decay = 0.1f, .sustain = 0.8f, .release = 0.1f,
    .start_frequency = 1, .end_frequency = 1,
    .amplitude = 0.07f, .sustain_level = 0.8f,
    .duty_cycle = 0.25f, .pan = 0.125f,
  },{
    .type = RIV_WAVEFORM_ORGAN,
    .attack = 0.1f, .decay = 0.1f, .sustain = 0.8f, .release = 0.1f,
    .start_frequency = 1, .end_frequency = 1,
    .amplitude = 0.07f, .sustain_level = 0.8f,
    .duty_cycle = 0.5f, .pan = -0.125f,
  }}};

  seqt_synth LEAD_WAVE = {.waves = {{
    .type = RIV_WAVEFORM_PULSE,
    .attack = 0.1f, .decay = 0.1f, .sustain = 0.6f, .release = 0.1f,
    .start_frequency = 2, .end_frequency = 2,
    .amplitude = 0.07f, .sustain_level = 0.75f,
    .duty_cycle = 0.125f, .pan = 0.125f,
  },{
    .type = RIV_WAVEFORM_TRIANGLE,
    .attack = 0.1f, .decay = 0.1f, .sustain = 0.6f, .release = 0.2f,
    .start_frequency = 4, .end_frequency = 4,
    .amplitude = 0.07f, .sustain_level = 0.75f,
    .duty_cycle = 0.5f, .pan = -0.125f,
  }}};

  seqt_synth BASS_WAVE = {.waves = {{
    .type = RIV_WAVEFORM_PULSE,
    .attack = 0.2f, .decay = 0.0f, .sustain = 0.7f, .release = 0.1f,
    .start_frequency = 0.25f, .end_frequency = 0.25f,
    .amplitude = 0.12f, .sustain_level = 1,
    .duty_cycle = 0.25f, .pan = 0.125f,
  },{
    .type = RIV_WAVEFORM_TILTED_SAWTOOTH,
    .attack = 0.2f, .decay = 0.0f, .sustain = 0.7f, .release = 0.1f,
    .start_frequency = 0.5f, .end_frequency = 0.5f,
    .amplitude = 0.08f, .sustain_level = 1,
    .duty_cycle = 0.5f, .pan = -0.125f,
  }}};

  // Drums
  seqt_synth HIGHKICK_WAVE = {.waves = {{
    .type = RIV_WAVEFORM_SINE,
    .attack = 0.05f, .decay = 0.05f, .sustain = 0.8f, .release = 0.1f,
    .start_frequency = RIV_NOTE_Eb3, .end_frequency = RIV_NOTE_C0,
    .amplitude = 0.4f, .sustain_level = 0.5f,
    .duty_cycle = 0.5f, .pan = 0.125f,
  },{
    .type = RIV_WAVEFORM_PULSE,
    .attack = 0.05f, .decay = 0.05f, .sustain = 0.8f, .release = 0.1f,
    .start_frequency = RIV_NOTE_Eb3, .end_frequency = RIV_NOTE_C0,
    .amplitude = 0.3f, .sustain_level = 0.5f,
    .duty_cycle = 0.2f, .pan = -0.125f,
  }}};

  seqt_synth KICK_WAVE = {.waves = {{
    .type = RIV_WAVEFORM_SINE,
    .attack = 0.05f, .decay = 0.05f, .sustain = 0.7f, .release = 0.1f,
    .start_frequency = RIV_NOTE_Eb3, .end_frequency = RIV_NOTE_C1,
    .amplitude = 0.5f, .sustain_level = 0.5f,
    .duty_cycle = 0.5f, .pan = 0.125f,
  },{
    .type = RIV_WAVEFORM_PULSE,
    .attack = 0.05f, .decay = 0.05f, .sustain = 0.7f, .release = 0.1f,
    .start_frequency = RIV_NOTE_Eb2, .end_frequency = RIV_NOTE_C1,
    .amplitude = 0.4f, .sustain_level = 0.5f,
    .duty_cycle = 0.2f, .pan = -0.125f,
  }}};

  seqt_synth LOWKICK_WAVE = {.waves = {{
    .type = RIV_WAVEFORM_SINE,
    .attack = 0.05f, .decay = 0.05f, .sustain = 0.7f, .release = 0.1f,
    .start_frequency = RIV_NOTE_Eb2, .end_frequency = RIV_NOTE_C0,
    .amplitude = 0.6f, .sustain_level = 0.5f,
    .duty_cycle = 0.5f, .pan = 0.125f,
  },{
    .type = RIV_WAVEFORM_PULSE,
    .attack = 0.05f, .decay = 0.05f, .sustain = 0.7f, .release = 0.1f,
    .start_frequency = RIV_NOTE_Eb2, .end_frequency = RIV_NOTE_C0,
    .amplitude = 0.5f, .sustain_level = 0.5f,
    .duty_cycle = 0.2f, .pan = -0.125f,
  }}};

  seqt_synth HIGHTOM_WAVE = {.waves = {{
    .type = RIV_WAVEFORM_SINE,
    .attack = 0.05f, .decay = 0.2f, .sustain = 0.7f, .release = 0.2f,
    .start_frequency = RIV_NOTE_C4, .end_frequency = RIV_NOTE_C0,
    .amplitude = 0.5f, .sustain_level = 0.4f,
    .duty_cycle = 0.2f, .pan = -0.125f,
  }}};

  seqt_synth TOM_WAVE = {.waves = {{
    .type = RIV_WAVEFORM_SINE,
    .attack = 0.05f, .decay = 0.2f, .sustain = 0.7f, .release = 0.2f,
    .start_frequency = RIV_NOTE_Eb3, .end_frequency = RIV_NOTE_C0,
    .amplitude = 0.6f, .sustain_level = 0.4f,
    .duty_cycle = 0.4f, .pan = 0.0f,
  }}};

  seqt_synth HIT_WAVE = {.waves = {{
    .type = RIV_WAVEFORM_NOISE,
    .attack = 0.02f, .decay = 0.1f, .sustain = 0.05f, .release = 0.05f,
    .start_frequency = RIV_NOTE_C7, .end_frequency = RIV_NOTE_C7,
    .amplitude = 0.08f, .sustain_level = 0.1f,
    .duty_cycle = 0.5f, .pan = 0.0f,
  }}};

  seqt_synth CLAP_WAVE = {.waves = {{
    .type = RIV_WAVEFORM_NOISE,
    .attack = 0.05f, .decay = 0.05f, .sustain = 0.3f, .release = 0.3f,
    .start_frequency = RIV_NOTE_C6, .end_frequency = RIV_NOTE_C6,
    .amplitude = 0.11f, .sustain_level = 0.3f,
    .duty_cycle = 0.5f, .pan = 0.0f,
  }}};

  seqt_synth CRASH_WAVE = {.waves = {{
    .type = RIV_WAVEFORM_NOISE,
    .attack = 0.05f, .decay = 0.05f, .sustain = 0.0f, .release = 0.9f,
    .start_frequency = RIV_NOTE_Eb6, .end_frequency = 2*RIV_NOTE_Eb8,
    .amplitude = 0.12f, .sustain_level = 0.4f,
    .duty_cycle = 0.5f, .pan = 0.0f,
  }}};

  seqt_synth CLICK_WAVE = {.waves = {{
    .type = RIV_WAVEFORM_TRIANGLE,
    .attack = 0.05f, .decay = 0.2f, .sustain = 0.1f, .release = 0.3f,
    .start_frequency = RIV_NOTE_Eb6, .end_frequency = RIV_NOTE_Eb6,
    .amplitude = 0.1f, .sustain_level = 0.2f,
    .duty_cycle = 0.5f, .pan = 0.0f,
  }}};

  seqt_synth HIGHCLICK_WAVE = {.waves = {{
    .type = RIV_WAVEFORM_TRIANGLE,
    .attack = 0.05f, .decay = 0.2f, .sustain = 0.2f, .release = 0.3f,
    .start_frequency = RIV_NOTE_Eb7, .end_frequency = RIV_NOTE_Eb7,
    .amplitude = 0.06f, .sustain_level = 0.3f,
    .duty_cycle = 0.5f, .pan = 0.0f,
  }}};

  // Synths
  seqt.default_font = (seqt_soundfont){.synths = {
    {STRINGS_WAVE,STRINGS_WAVE,STRINGS_WAVE,STRINGS_WAVE,STRINGS_WAVE,STRINGS_WAVE,STRINGS_WAVE,STRINGS_WAVE,STRINGS_WAVE,STRINGS_WAVE},
    {LEAD_WAVE,LEAD_WAVE,LEAD_WAVE,LEAD_WAVE,LEAD_WAVE,LEAD_WAVE,LEAD_WAVE,LEAD_WAVE,LEAD_WAVE,LEAD_WAVE},
    {BASS_WAVE,BASS_WAVE,BASS_WAVE,BASS_WAVE,BASS_WAVE,BASS_WAVE,BASS_WAVE,BASS_WAVE,BASS_WAVE,BASS_WAVE},
    {HIGHKICK_WAVE,KICK_WAVE,LOWKICK_WAVE,HIGHTOM_WAVE,TOM_WAVE,HIT_WAVE,CLAP_WAVE,CRASH_WAVE,HIGHCLICK_WAVE,CLICK_WAVE},
  }};

  // Scale
  int scale_semitone_index = 39; // Eb
  seqt_fill_major_pentatonic_scale(seqt.default_font.scale, scale_semitone_index);
}

#endif // SEQT_IMPL
