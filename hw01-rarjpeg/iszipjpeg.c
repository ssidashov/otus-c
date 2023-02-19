#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#pragma pack(push, 2)

struct lfh
{
    // Обязательная сигнатура, равна 0x04034b50
    uint32_t signature;
    // Минимальная версия для распаковки
    uint16_t versionToExtract;
    // Битовый флаг
    uint16_t generalPurposeBitFlag;
    // Метод сжатия (0 - без сжатия, 8 - deflate)
    uint16_t compressionMethod;
    // Время модификации файла
    uint16_t modificationTime;
    // Дата модификации файла
    uint16_t modificationDate;
    // Контрольная сумма
    uint32_t crc32;
    // Сжатый размер
    uint32_t compressedSize;
    // Несжатый размер
    uint32_t uncompressedSize;
    // Длина название файла
    uint16_t filenameLength;
    // Длина поля с дополнительными данными
    uint16_t extraFieldLength;
    // Название файла (размером filenameLength)
    char *filename;
    // Дополнительные данные (размером extraFieldLength)
    char *extraField;
};

struct cfh {
        uint16_t made_by_ver;    /* Version made by. */
        uint16_t extract_ver;    /* Version needed to extract. */
        uint16_t gp_flag;        /* General purpose bit flag. */
        uint16_t method;         /* Compression method. */
        uint16_t mod_time;       /* Modification time. */
        uint16_t mod_date;       /* Modification date. */
        uint32_t crc32;          /* CRC-32 checksum. */
        uint32_t comp_size;      /* Compressed size. */
        uint32_t uncomp_size;    /* Uncompressed size. */
        uint16_t name_len;       /* Filename length. */
        uint16_t extra_len;      /* Extra data length. */
        uint16_t comment_len;    /* Comment length. */
        uint16_t disk_nbr_start; /* Disk nbr. where file begins. */
        uint16_t int_attrs;      /* Internal file attributes. */
        uint32_t ext_attrs;      /* External file attributes. */
        uint32_t lfh_offset;     /* Local File Header offset. */
        const uint8_t *name;     /* Filename. */
        const uint8_t *extra;    /* Extra data. */
        const uint8_t *comment;  /* File comment. */
};

struct eocdr {
        uint16_t disk_nbr;        /* Number of this disk. */
        uint16_t cd_start_disk;   /* Nbr. of disk with start of the CD. */
        uint16_t disk_cd_entries; /* Nbr. of CD entries on this disk. */
        uint16_t cd_entries;      /* Nbr. of Central Directory entries. */
        uint32_t cd_size;         /* Central Directory size in bytes. */
        uint32_t cd_offset;       /* Central Directory file offset. */
        uint16_t comment_len;     /* Archive comment length. */
        const uint8_t *comment;   /* Archive comment. */
};

typedef struct {
  struct lfh **array;
  size_t size;
  size_t buf_size;
} Array;

#pragma pack(pop)

static uint8_t const lfh_signature[4] = { 0x50, 0x4b, 0x03, 0x04 };
//static uint8_t const cfh_signature[4] = {0x50, 0x4b, 0x01, 0x02};
//static uint8_t const eocdr_signature[4] = {0x50, 0x4b, 0x05, 0x06};

//static int zip_start_position;

static FILE* curr_file_input_desc;
static char const *infile;

void initArray(Array *a, size_t initialSize) {
  a->array = malloc(initialSize * sizeof(struct lfh*));
  a->size = 0;
  a->buf_size = initialSize;
}

void insertArray(Array *a, struct lfh *element) {
  // a->used is the number of used entries, because a->array[a->used++] updates a->used only *after* the array has been accessed.
  // Therefore a->used can go up to a->size
  if (a->size == a->buf_size) {
    a->buf_size *= 2;
    a->array = realloc(a->array, a->buf_size * sizeof(struct lfh*));
  }
  a->array[a->size++] = element;
}

void freeArray(Array *a) {
  free(a->array);
  a->array = NULL;
  a->size = a->buf_size = 0;
}

void print_usage(const char *app_name);
void print_wrong_args();
bool check_file_is_rarjpeg(FILE *file, const char *filename);
bool is_jpeg(uint8_t buffer[], size_t readed_size);
unsigned long found_chf_or_zip_sig_format(uint8_t buffer[], unsigned long start_from, size_t biffer_size, unsigned short *validated_signature_index);
void print_zip_content(long offset, Array found_list);
bool fread_prebuffered(void* ptr,
        size_t size,
        size_t memsize,
        FILE* file,
        uint8_t buffer[],
        size_t buffer_size,
        unsigned *current_buffer_position,
        size_t *readed_from_file);
void release_resources(Array *list);

int main (int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "-h") == 0)
    {
        print_usage(argv[0]);
        return 0;
    }

    if(argc == 1 && isatty(STDIN_FILENO))
    {
        print_usage(argv[0]);
        return 1;
    }

    bool all_files_check_result = true;
    bool was_error = false;
    infile = "-";
    int current_file_arg_index = 1;

    do
    {
        if  (current_file_arg_index < argc)
            infile = argv[current_file_arg_index];
        bool current_file_is_stdin = strcmp(infile, "-") == 0;
        if (current_file_is_stdin)
        {
            curr_file_input_desc = freopen(NULL, "rb", stdin);
        }
        else
        {
            curr_file_input_desc = fopen(infile, "rb");
            if (curr_file_input_desc == NULL)
            {
                fprintf(stderr, "Cannot open file %s: ", infile);
                perror(NULL);
                was_error = true;
                continue;
            }
        }
        all_files_check_result = check_file_is_rarjpeg(curr_file_input_desc, current_file_is_stdin ? NULL : infile);
        if (!current_file_is_stdin && fclose(curr_file_input_desc) < 0)
        {
            fprintf(stderr, "Error closing file %s", infile);
            was_error = true;
        }
    } while (++current_file_arg_index < argc);

    if(was_error)
    {
        return 2;
    }

    return all_files_check_result ? 0 : 1;
}

void print_usage(const char *app_name)
{
    printf("Usage: %s <file_to_test1>...\n", app_name);
}

bool check_file_is_rarjpeg(FILE *file, const char *filename)
{
    if(filename != NULL)
        printf("file %s:", filename);

    const size_t buffer_size = 512;
    uint8_t buffer[buffer_size];

    size_t readed_size = fread(&buffer, sizeof(uint8_t), 3, file);

    if(!is_jpeg(buffer, readed_size))
    {
        printf("is not even JPEG\n");
        return false;
    };

    unsigned short validated_signature_index = 0;
    long zip_start_position = -1;
    unsigned long current_file_position = 0;
    unsigned int current_buffer_position = buffer_size;
    Array found_lfhs;
    initArray(&found_lfhs, 128);
    do
    {
        if(current_buffer_position >= buffer_size-1)
        {
            current_file_position += readed_size;
            current_buffer_position = 0;
            readed_size = fread(&buffer, sizeof(uint8_t), buffer_size, file);
        }
        int found_offset = found_chf_or_zip_sig_format(buffer, current_buffer_position, readed_size, &validated_signature_index);
        if (found_offset < 0)
        {
            current_buffer_position = buffer_size-1;
            continue;
        }
        if(zip_start_position == -1)
        {
            zip_start_position = current_file_position + current_buffer_position + 1 + found_offset - 4;
        }
        current_buffer_position += found_offset + 1;
        struct lfh *current_lfh = malloc(sizeof(struct lfh));
        memcpy(current_lfh, lfh_signature, sizeof(uint32_t));
        size_t readed_lfh_bytes = 0;
        bool readed_full = fread_prebuffered(((uint8_t*)current_lfh) + 4,
                sizeof(struct lfh) - sizeof(uint32_t) - sizeof(uint8_t*)*2,
                1,
                file,
                buffer,
                buffer_size,
                &current_buffer_position,
                &readed_lfh_bytes);
        if(readed_lfh_bytes > 0)
        {
            current_file_position += readed_lfh_bytes;
        }
        if(!readed_full)
        {
            continue;
        }
        char* fname_buffer = malloc(current_lfh->filenameLength + 1);
        readed_full = fread_prebuffered(fname_buffer,
            sizeof(uint8_t),
            current_lfh->filenameLength,
            file,
            buffer,
            buffer_size,
            &current_buffer_position,
            &readed_lfh_bytes);
         if(readed_lfh_bytes > 0)
        {
            current_file_position += readed_lfh_bytes;
        }
        if(!readed_full)
        {
            continue;
        }
        fname_buffer[current_lfh->filenameLength] = 0;
        current_lfh->filename = fname_buffer;
        validated_signature_index = 0;
        insertArray(&found_lfhs, current_lfh);
    } while (readed_size == buffer_size);

    if(zip_start_position >= 0)
    {
        print_zip_content(zip_start_position, found_lfhs);
    } else
    {
        printf("is not ZIPJPEG\n");
    }
    release_resources(&found_lfhs);
    return zip_start_position >= 0;
}

bool is_jpeg(uint8_t buffer[], size_t readed_size)
{
   return !(readed_size != 3 || !(buffer[0] == 0xff
        && buffer[1] == 0xd8
        && buffer[2] == 0xff));
}

unsigned long found_chf_or_zip_sig_format(uint8_t buffer[], unsigned long start_from, size_t buffer_size, unsigned short *validated_signature_index)
{
    unsigned long i;
    for(i = start_from; i < buffer_size; i++)
    {
        if(lfh_signature[*validated_signature_index] == buffer[i])
        {
            (*validated_signature_index)++;
        } else
        {
            *validated_signature_index = 0;
        }
        if(*validated_signature_index == 4)
            break;
    }
    if(*validated_signature_index == 4)
    {
        return i - start_from;
    }
    return -1;
}

bool fread_prebuffered(void* ptr, size_t size, size_t memsize, FILE* file, uint8_t buffer[], size_t buffer_size, unsigned *current_buffer_position, size_t *readed_from_file)
{
    unsigned aval_from_buf = buffer_size - *current_buffer_position;
    unsigned to_read_from_buffer = aval_from_buf > size * memsize ? size * memsize : aval_from_buf;
    unsigned to_read_from_file = size * memsize - to_read_from_buffer;
    memcpy(ptr, &buffer[*current_buffer_position], to_read_from_buffer);
    *current_buffer_position += to_read_from_buffer;
    if(to_read_from_file > 0)
    {
        size_t readed = fread(((uint8_t*)ptr) + to_read_from_buffer, to_read_from_file, 1, file);
        if(readed < to_read_from_file)
        {
            return false;
        }
        *readed_from_file = readed;
        return true;
    } else
    {
        *readed_from_file = 0;
    }
    return true;
}

void print_zip_content(long offset, Array found_lfhs)
{
    printf("ZIP found at %ld, file count: %zu\n", offset, found_lfhs.size);
    for(unsigned i = 0; i < found_lfhs.size; i++)
    {
        printf("%s\n", found_lfhs.array[i]->filename);
    }
}

void release_resources(Array *list)
{
    for(unsigned i = 0; i < list->size; i++)
    {
        free(list->array[i]->filename);
        free(list->array[i]);
    }
    freeArray(list);
}
