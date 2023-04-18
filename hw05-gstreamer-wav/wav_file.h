#ifndef _WAV_FILE_H_
#define _WAV_FILE_H_

#include <glib-object.h>

#include <stdint.h>

#define WAV_FORMAT_PCM ((uint16_t)0x0001)
#define WAV_FORMAT_IEEE_FLOAT ((uint16_t)0x0003)
#define WAV_FORMAT_ALAW ((uint16_t)0x0006)
#define WAV_FORMAT_MULAW ((uint16_t)0x0007)
#define WAV_FORMAT_EXTENSIBLE ((uint16_t)0xfffe)

#define WAV_FILE_ERRCODE_FILE_OPERATION_ERROR -1
#define WAV_FILE_ERRCODE_FILE_FORMAT_ERROR -2
#define WAV_FILE_ERRCODE_WRONG_FILENAME -3
#define WAV_FILE_ERRCODE_WRONG_STATE -4
#define WAV_FILE_ERRCODE_FORMAT_NOT_SUPPORTED -5
#define WAV_FILE_ERRCODE_ILLEGAL_PARAMETER -6

G_BEGIN_DECLS

#define TYPE_WAV_FILE (wav_file_get_type())

G_DECLARE_DERIVABLE_TYPE(WavFile, wav_file, WAV, FILE, GObject)

typedef struct _WavFile WavFile;
typedef struct _WavFileClass WavFileClass;

int wav_file_new(const gchar *filename, WavFile **target);
void wav_file_close(WavFile *self);
gssize wav_file_read_samples(WavFile *self, void *buffer, gsize count);
uint16_t wav_file_get_format(WavFile *self);
uint16_t wav_file_get_channel_count(WavFile *self);
gsize wav_file_get_sample_size(WavFile *self);
gssize wav_file_tell(WavFile *self);
gsize wav_file_get_length(WavFile *self);
gssize wav_file_seek(WavFile *self, gssize offset, int origin);
uint32_t wav_file_get_sample_rate(WavFile *self);
uint16_t wav_file_get_block_align(WavFile *self);
gboolean wav_file_eof(WavFile *self);

struct _WavFileClass {
  GObjectClass parent_class;
  int (*init)(WavFile *self, const gchar *filename);
  void (*close)(WavFile *self);
  gssize (*read_samples)(WavFile *self, void *buffer, gsize count);
  uint16_t (*get_format)(WavFile *self);
  uint16_t (*get_channel_count)(WavFile *self);
  gsize (*get_length)(WavFile *self);
  gssize (*tell)(WavFile *self);
  gssize (*seek)(WavFile *self, gssize offset, int origin);
  uint32_t (*get_sample_rate)(WavFile *self);
  gsize (*get_sample_size)(WavFile *self);
  uint16_t (*get_block_align)(WavFile *self);
  gboolean (*eof)(WavFile *self);
};

GType wav_file_get_type(void);

G_END_DECLS

#endif
