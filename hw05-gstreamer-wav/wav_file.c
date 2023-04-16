#include "wav_file.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <glib/gstdio.h>

#define WAV_RIFF_CHUNK_ID ((uint32_t)'FFIR')
#define WAV_FORMAT_CHUNK_ID ((uint32_t)' tmf')
#define WAV_FACT_CHUNK_ID ((uint32_t)'tcaf')
#define WAV_DATA_CHUNK_ID ((uint32_t)'atad')
#define WAV_WAVE_ID ((uint32_t)'EVAW')

#define WAV_CHUNK_MASTER ((uint32_t)1)
#define WAV_CHUNK_FORMAT ((uint32_t)2)
#define WAV_CHUNK_FACT ((uint32_t)4)
#define WAV_CHUNK_DATA ((uint32_t)8)

#define WAV_FILE_ERRCODE_FILE_OPERATION_ERROR -1
#define WAV_FILE_ERRCODE_FILE_FORMAT_ERROR -2
#define WAV_FILE_ERRCODE_WRONG_FILENAME -3
#define WAV_FILE_ERRCODE_WRONG_STATE -4
#define WAV_FILE_ERRCODE_FORMAT_NOT_SUPPORTED -5
#define WAV_FILE_ERRCODE_ILLEGAL_PARAMETER -6

#pragma pack(push, 1)

typedef struct {
  uint32_t id;
  uint32_t size;
} WavChunkHeader;

typedef struct {
  WavChunkHeader header;

  uint64_t offset;

  struct {
    uint16_t format_tag;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t avg_bytes_per_sec;
    uint16_t block_align;
    uint16_t bits_per_sample;

    uint16_t ext_size;
    uint16_t valid_bits_per_sample;
    uint32_t channel_mask;

    uint8_t sub_format[16];
  } body;
} WavFormatChunk;

typedef struct {
  WavChunkHeader header;

  uint64_t offset;

  struct {
    uint32_t sample_length;
  } body;
} WavFactChunk;

typedef struct {
  WavChunkHeader header;
  uint64_t offset;
} WavDataChunk;

typedef struct {
  uint32_t id;
  uint32_t size;
  uint32_t wave_id;
  uint64_t offset;
} WavMasterChunk;

#pragma pack(pop)

typedef struct {
  GObject base_class;
  FILE *file_handle;
  gchar *filename;

  bool is_opened;

  WavMasterChunk riff_chunk;
  WavFormatChunk format_chunk;
  WavFactChunk fact_chunk;
  WavDataChunk data_chunk;
} WavFilePrivate;

int wav_file_init_impl(WavFile *self, const gchar *filename);
void wav_file_close_impl(WavFile *self);
void wav_file_dispose(GObject *object);
void wav_file_finalize(GObject *object);
uint16_t wav_file_get_format_impl(WavFile *self);
uint16_t wav_file_get_channel_count_impl(WavFile *self);
gsize wav_file_get_sample_size_impl(WavFile *self);
gssize wav_file_tell_impl(WavFile *self);
gsize wav_file_get_length_impl(WavFile *self);
gssize wav_file_seek_impl(WavFile *self, gssize offset, int origin);
uint32_t wav_file_get_sample_rate_impl(WavFile *self);
gssize wav_file_read_samples_impl(WavFile *self, void *buffer, gsize count);
gboolean wav_file_eof_impl(WavFile *self);

G_DEFINE_TYPE_WITH_PRIVATE(WavFile, wav_file, G_TYPE_OBJECT)

static void wav_file_class_init(WavFileClass *self) {
  GObjectClass *gobject_class = G_OBJECT_CLASS(self);

  self->init = wav_file_init_impl;
  self->close = wav_file_close_impl;

  self->read_samples = wav_file_read_samples_impl;
  self->get_format = wav_file_get_format_impl;
  self->get_channel_count = wav_file_get_channel_count_impl;
  self->get_sample_size = wav_file_get_sample_size_impl;
  self->tell = wav_file_tell_impl;
  self->get_length = wav_file_get_length_impl;
  self->seek = wav_file_seek_impl;
  self->get_sample_rate = wav_file_get_sample_rate_impl;
  self->eof = wav_file_eof_impl;
  gobject_class->dispose = wav_file_dispose;
  gobject_class->finalize = wav_file_finalize;
}

static void wav_file_init(__attribute__((unused)) WavFile *self) {
  g_debug("created wav file");
}

int wav_header_parse(WavFilePrivate *wavFileProps) {
  gsize read_count;

  read_count = fread(&wavFileProps->riff_chunk, sizeof(WavChunkHeader), 1,
                     wavFileProps->file_handle);
  if (read_count != 1) {
    g_error("Unexpected end of file while reading wav header");
    return WAV_FILE_ERRCODE_FILE_FORMAT_ERROR;
  }

  if (wavFileProps->riff_chunk.id != WAV_RIFF_CHUNK_ID) {
    g_error("File is not a RIFF file");
    return WAV_FILE_ERRCODE_FILE_FORMAT_ERROR;
  }

  read_count =
      fread(&wavFileProps->riff_chunk.wave_id, 4, 1, wavFileProps->file_handle);
  if (read_count != 1) {
    g_error("Unexpected end of file while reading wav header");
    return WAV_FILE_ERRCODE_FILE_FORMAT_ERROR;
  }
  if (wavFileProps->riff_chunk.wave_id != WAV_WAVE_ID) {
    g_error("File is not a WAV file");
    return WAV_FILE_ERRCODE_FILE_FORMAT_ERROR;
  }

  wavFileProps->riff_chunk.offset = (uint64_t)ftell(wavFileProps->file_handle);

  while (wavFileProps->data_chunk.header.id != WAV_DATA_CHUNK_ID) {
    WavChunkHeader header;

    read_count =
        fread(&header, sizeof(WavChunkHeader), 1, wavFileProps->file_handle);
    if (read_count != 1) {
      g_error("Unexpected end of file while reading wav header");
      return WAV_FILE_ERRCODE_FILE_FORMAT_ERROR;
    }

    switch (header.id) {
    case WAV_FORMAT_CHUNK_ID:
      wavFileProps->format_chunk.header = header;
      wavFileProps->format_chunk.offset =
          (uint64_t)ftell(wavFileProps->file_handle);
      read_count = fread(&wavFileProps->format_chunk.body, header.size, 1,
                         wavFileProps->file_handle);
      if (read_count != 1) {
        g_error("Unexpected end of file while reading wav header");
        return WAV_FILE_ERRCODE_FILE_FORMAT_ERROR;
      }
      if (wavFileProps->format_chunk.body.format_tag != WAV_FORMAT_PCM &&
          wavFileProps->format_chunk.body.format_tag != WAV_FORMAT_IEEE_FLOAT &&
          wavFileProps->format_chunk.body.format_tag != WAV_FORMAT_ALAW &&
          wavFileProps->format_chunk.body.format_tag != WAV_FORMAT_MULAW) {
        g_error("Unknow format in format tag: %#010x",
                wavFileProps->format_chunk.body.format_tag);
        return WAV_FILE_ERRCODE_FILE_FORMAT_ERROR;
      }
      break;
    case WAV_FACT_CHUNK_ID:
      wavFileProps->fact_chunk.header = header;
      wavFileProps->fact_chunk.offset =
          (uint64_t)ftell(wavFileProps->file_handle);
      read_count = fread(&wavFileProps->fact_chunk.body, header.size, 1,
                         wavFileProps->file_handle);
      if (read_count != 1) {
        g_error("Unexpected end of file while reading wav header");
        return WAV_FILE_ERRCODE_FILE_FORMAT_ERROR;
      }
      break;
    case WAV_DATA_CHUNK_ID:
      wavFileProps->data_chunk.header = header;
      wavFileProps->data_chunk.offset =
          (uint64_t)ftell(wavFileProps->file_handle);
      break;
    default:
      if (fseek(wavFileProps->file_handle, header.size, SEEK_CUR) < 0) {
        g_error("seek failed, code %d: %s", errno, strerror(errno));
        return WAV_FILE_ERRCODE_FILE_OPERATION_ERROR;
      }
      break;
    }
  }
  return 0;
}

int wav_file_init_impl(WavFile *self, const gchar *filename) {
  WavFilePrivate *priv = wav_file_get_instance_private(self);
  if (filename == NULL) {
    return WAV_FILE_ERRCODE_WRONG_FILENAME;
  }
  priv->riff_chunk = (WavMasterChunk){0};
  priv->format_chunk = (WavFormatChunk){0};
  priv->fact_chunk = (WavFactChunk){0};
  priv->data_chunk = (WavDataChunk){0};

  priv->file_handle = g_fopen(filename, "rb");
  if (priv->file_handle == NULL) {
    g_error("Cannot open wav file %s, errno %d: %s", filename, errno,
            strerror(errno));
    return WAV_FILE_ERRCODE_FILE_OPERATION_ERROR;
  }
  priv->filename = g_strdup(filename);
  int parse_error_code = wav_header_parse(priv);
  if (parse_error_code != 0) {
    return parse_error_code;
  }
  priv->is_opened = true;
  return 0;
}

gssize wav_file_read_samples_impl(WavFile *self, void *buffer, gsize count) {
  WavFilePrivate *priv = wav_file_get_instance_private(self);
  gsize read_count;
  uint16_t n_channels = wav_file_get_channel_count_impl(self);
  gsize sample_size = wav_file_get_sample_size_impl(self);
  gsize len_remain;

  if (!priv->is_opened) {
    g_error("Wav file was not opened");
    return WAV_FILE_ERRCODE_WRONG_STATE;
  }

  if (priv->format_chunk.body.format_tag == WAV_FORMAT_EXTENSIBLE) {
    g_error("Extensible format is not supported");
    return WAV_FILE_ERRCODE_FORMAT_NOT_SUPPORTED;
  }
  gssize pos = wav_file_tell_impl(self);
  if (pos < 0) {
    return pos;
  }

  len_remain = wav_file_get_length_impl(self) - pos;
  count = (count <= len_remain) ? count : len_remain;

  if (count == 0) {
    return 0;
  }

  read_count =
      fread(buffer, sample_size, n_channels * count, priv->file_handle);
  if (ferror(priv->file_handle)) {
    g_error("Error reading samples from file %s - %d %s", priv->filename, errno,
            strerror(errno));
    return WAV_FILE_ERRCODE_FILE_OPERATION_ERROR;
  }

  return read_count / n_channels;
}

WavFile *wav_file_new(const gchar *filename) {
  WavFile *object = g_object_new(TYPE_WAV_FILE, NULL);
  WavFileClass *klass = WAV_FILE_GET_CLASS(object);
  int result = klass->init(object, filename);
  if (result != 0) {
    g_object_unref(object);
    return NULL;
  }
  return object;
}

void wav_file_close(WavFile *self) {
  WavFileClass *klass = WAV_FILE_GET_CLASS(self);
  klass->close(self);
}

gssize wav_file_read_samples(WavFile *self, void *buffer, gsize count) {
  WavFileClass *klass = WAV_FILE_GET_CLASS(self);
  return klass->read_samples(self, buffer, count);
}

uint16_t wav_file_get_format(WavFile *self) {
  WavFileClass *klass = WAV_FILE_GET_CLASS(self);
  return klass->get_format(self);
}

uint16_t wav_file_get_channel_count(WavFile *self) {
  WavFileClass *klass = WAV_FILE_GET_CLASS(self);
  return klass->get_channel_count(self);
}

gsize wav_file_get_sample_size(WavFile *self) {
  WavFileClass *klass = WAV_FILE_GET_CLASS(self);
  return klass->get_sample_size(self);
}

gssize wav_file_tell(WavFile *self) {
  WavFileClass *klass = WAV_FILE_GET_CLASS(self);
  return klass->tell(self);
}

gsize wav_file_get_length(WavFile *self) {
  WavFileClass *klass = WAV_FILE_GET_CLASS(self);
  return klass->get_length(self);
}
gssize wav_file_seek(WavFile *self, gssize offset, int origin) {
  WavFileClass *klass = WAV_FILE_GET_CLASS(self);
  return klass->seek(self, offset, origin);
}

uint32_t wav_file_get_sample_rate(WavFile *self) {
  WavFileClass *klass = WAV_FILE_GET_CLASS(self);
  return klass->get_sample_rate(self);
}

gboolean wav_file_eof(WavFile *self) {
  WavFileClass *klass = WAV_FILE_GET_CLASS(self);
  return klass->eof(self);
}

void wav_file_close_impl(WavFile *self) {
  WavFilePrivate *priv = wav_file_get_instance_private(self);
  if (!priv->is_opened) {
    return;
  }

  int ret;

  ret = fclose(priv->file_handle);
  if (ret != 0) {
    g_error("Error closing file %s - %d %s", priv->filename, errno,
            strerror(errno));
    return;
  }

  priv->is_opened = false;
}

uint16_t wav_file_get_channel_count_impl(WavFile *self) {
  WavFilePrivate *priv = wav_file_get_instance_private(self);
  return priv->format_chunk.body.num_channels;
}
gsize wav_file_get_sample_size_impl(WavFile *self) {
  WavFilePrivate *priv = wav_file_get_instance_private(self);
  return priv->format_chunk.body.block_align /
         priv->format_chunk.body.num_channels;
}
gssize wav_file_tell_impl(WavFile *self) {
  WavFilePrivate *priv = wav_file_get_instance_private(self);
  long pos = ftell(priv->file_handle);

  if (pos == -1L) {
    g_error("Error telling file %s - %d %s", priv->filename, errno,
            strerror(errno));
    return WAV_FILE_ERRCODE_FILE_OPERATION_ERROR;
  }

  g_assert(pos >= (long)priv->data_chunk.offset);

  return (long)(((uint64_t)pos - priv->data_chunk.offset) /
                (priv->format_chunk.body.block_align));
}
gsize wav_file_get_length_impl(WavFile *self) {
  WavFilePrivate *priv = wav_file_get_instance_private(self);
  return priv->data_chunk.header.size / (priv->format_chunk.body.block_align);
}
uint16_t wav_file_get_format_impl(WavFile *self) {
  WavFilePrivate *priv = wav_file_get_instance_private(self);
  return priv->format_chunk.body.format_tag;
}

gssize wav_file_seek_impl(WavFile *self, gssize offset, int origin) {
  WavFilePrivate *priv = wav_file_get_instance_private(self);
  size_t length = wav_file_get_length_impl(self);
  int ret;

  if (origin == SEEK_CUR) {
    gssize tell_result = wav_file_tell_impl(self);
    if(tell_result < 0) {
        return tell_result;
    }
    offset += (long)tell_result;
  } else if (origin == SEEK_END) {
    offset += (long)length;
  }

  if (offset >= 0) {
    offset *= priv->format_chunk.body.block_align;
  } else {
    g_error("Invalid seek: %ld", offset);
    return WAV_FILE_ERRCODE_ILLEGAL_PARAMETER;
  }

  ret = fseek(priv->file_handle, (long)priv->data_chunk.offset + offset,
              SEEK_SET);

  if (ret != 0) {
    g_error("Failed seeking file %s - %d %s", priv->filename, errno,
            strerror(errno));
    return WAV_FILE_ERRCODE_FILE_OPERATION_ERROR;
  }

  return 0;
}

uint32_t wav_file_get_sample_rate_impl(WavFile *self) {
  WavFilePrivate *priv = wav_file_get_instance_private(self);
  return priv->format_chunk.body.sample_rate;
}

gboolean wav_file_eof_impl(WavFile *self) {
  WavFilePrivate *priv = wav_file_get_instance_private(self);
  if (!priv->is_opened) {
    return true;
  }
  return feof(priv->file_handle) ||
         ftell(priv->file_handle) ==
             (long)(priv->data_chunk.offset + priv->data_chunk.header.size);
}

void wav_file_dispose(GObject *object) {
  // WavFilePrivate *priv = wav_file_get_instance_private(WAV_FILE(object));

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS(wav_file_parent_class)->dispose(object);
}

void wav_file_finalize(GObject *object) {
  /* clean up object here */
  WavFile *wavFile = WAV_FILE(object);
  WavFilePrivate *priv = wav_file_get_instance_private(wavFile);

  WavFileClass *klass = WAV_FILE_GET_CLASS(wavFile);
  klass->close(wavFile);

  g_free(priv->filename);
  priv->filename = NULL;

  G_OBJECT_CLASS(wav_file_parent_class)->finalize(object);
}
