#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#define LOG_LEVEL_FATAL   0
#define LOG_LEVEL_ERROR   1
#define LOG_LEVEL_WARN    2
#define LOG_LEVEL_INFO    3
#define LOG_LEVEL_DEBUG   4
#define LOG_LEVEL_TRACE   5

#ifndef CURRENT_LOG_LEVEL
#define CURRENT_LOG_LEVEL LOG_LEVEL_TRACE
#endif

#define MAX_LOG_SIZE 4000

// Logging configuration
typedef enum {
    LOG_OUTPUT_NONE = 0,
    LOG_OUTPUT_STDOUT = 1,
    LOG_OUTPUT_FILE = 2
} log_output_t;

static log_output_t current_log_output = LOG_OUTPUT_STDOUT;
static FILE* log_file = NULL;
static char log_buffer[MAX_LOG_SIZE];
static size_t log_size = 0;

static inline void syslog_print(const char* message) {
    if (!message) return;

    int i = 0;
    while (message[i] != '\0' && log_size < MAX_LOG_SIZE - 1) {
        log_buffer[log_size++] = message[i++];
    }
    log_buffer[log_size] = '\0';
}

// Configure logging output
static inline void log_set_output(log_output_t output, const char* filename) {
    // Close existing file if any
    if (log_file && log_file != stdout) {
        fclose(log_file);
        log_file = NULL;
    }

    current_log_output = output;

    if (output == LOG_OUTPUT_FILE && filename) {
        log_file = fopen(filename, "a");
        if (!log_file) {
            // Fallback to stdout if file can't be opened
            current_log_output = LOG_OUTPUT_STDOUT;
            log_file = stdout;
        }
    } else if (output == LOG_OUTPUT_STDOUT) {
        log_file = stdout;
    } else {
        log_file = NULL;
    }
}

// Helper functions
static inline void reverse(char* str, int len) {
    int start = 0;
    int end = len - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

static inline char* itoa(int num, char* str, int base) {
    int i = 0;
    bool isNegative = false;

    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    if (num < 0 && base == 10) {
        isNegative = true;
        num = -num;
    }

    while (num != 0) {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num /= base;
    }

    if (isNegative) {
        str[i++] = '-';
    }

    str[i] = '\0';
    reverse(str, i);
    return str;
}

static inline char* utoa_hex(uintptr_t num, char* str) {
    int i = 0;

    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    while (num != 0) {
        int rem = num % 16;
        str[i++] = (rem > 9) ? (rem - 10) + 'A' : rem + '0';
        num /= 16;
    }

    str[i] = '\0';
    reverse(str, i);
    return str;
}

static inline void log_format_basic(const char* level, const char* format, ...) {
    char buffer[256];
    char temp_buf[32];
    int buf_pos = 0;
    
    buffer[buf_pos++] = '[';
    const char* l = level;
    while (*l) {
        buffer[buf_pos++] = *l++;
    }
    buffer[buf_pos++] = ']';
    buffer[buf_pos++] = ' ';

    va_list args;
    va_start(args, format);
    
    const char* fmt = format;
    while (*fmt && buf_pos < 250) {
        if (*fmt == '%' && *(fmt + 1) == 'd') {
            int value = va_arg(args, int);
            itoa(value, temp_buf, 10);
            
            char* n = temp_buf;
            while (*n) {
                buffer[buf_pos++] = *n++;
            }
            
            fmt += 2;
        } else if (*fmt == '%' && *(fmt + 1) == 'u') {
            unsigned int value = va_arg(args, unsigned int);
            itoa((int)value, temp_buf, 10);
            
            char* n = temp_buf;
            while (*n) {
                buffer[buf_pos++] = *n++;
            }
            
            fmt += 2;
        } else if (*fmt == '%' && *(fmt + 1) == 'x') {
            unsigned int value = va_arg(args, unsigned int);
            itoa((int)value, temp_buf, 16);
            
            char* n = temp_buf;
            while (*n) {
                buffer[buf_pos++] = *n++;
            }
            
            fmt += 2;
        } else if (*fmt == '%' && *(fmt + 1) == 'X') {
            unsigned int value = va_arg(args, unsigned int);
            utoa_hex(value, temp_buf); 
            
            char* n = temp_buf;
            while (*n) {
                buffer[buf_pos++] = *n++;
            }
            
            fmt += 2;
        } else if (*fmt == '%' && *(fmt + 1) == 's') {
            const char* str = va_arg(args, const char*);
            while (*str) {
                buffer[buf_pos++] = *str++;
            }
            
            fmt += 2;
        } else if (*fmt == '%' && *(fmt + 1) == 'c') {
            char c = (char)va_arg(args, int);
            buffer[buf_pos++] = c;
            
            fmt += 2;
        } else if (*fmt == '%' && *(fmt + 1) == 'p') {
            void* ptr = va_arg(args, void*);
            buffer[buf_pos++] = '0';
            buffer[buf_pos++] = 'x';

            uintptr_t addr = (uintptr_t)ptr;
            utoa_hex(addr, temp_buf);
            
            int len = 0;
            while (temp_buf[len]) len++;
     
            for (int i = len; i < 8; i++) {
                buffer[buf_pos++] = '0';
            }
            
            char* n = temp_buf;
            while (*n) {
                buffer[buf_pos++] = *n++;
            }
            
            fmt += 2;
        } else if (*fmt == '%' && *(fmt + 1) == '%') {
            buffer[buf_pos++] = '%';
            fmt += 2;
        } else {
            buffer[buf_pos++] = *fmt++;
        }
    }
    
    va_end(args);

    buffer[buf_pos] = '\0';

    if (current_log_output != LOG_OUTPUT_NONE && log_file) {
        fprintf(log_file, "%s", buffer);
        fflush(log_file);
    }
}

#define LOG_FATAL(...) do { if (LOG_LEVEL_FATAL <= CURRENT_LOG_LEVEL) log_format_basic("FATAL", __VA_ARGS__); } while(0)
#define LOG_ERROR(...) do { if (LOG_LEVEL_ERROR <= CURRENT_LOG_LEVEL) log_format_basic("ERROR", __VA_ARGS__); } while(0)
#define LOG_WARN(...)  do { if (LOG_LEVEL_WARN <= CURRENT_LOG_LEVEL)  log_format_basic("WARN", __VA_ARGS__); } while(0)
#define LOG_INFO(...)  do { if (LOG_LEVEL_INFO <= CURRENT_LOG_LEVEL)  log_format_basic("INFO", __VA_ARGS__); } while(0)
#define LOG_DEBUG(...) do { if (LOG_LEVEL_DEBUG <= CURRENT_LOG_LEVEL) log_format_basic("DEBUG", __VA_ARGS__); } while(0)
#define LOG_TRACE(...) do { if (LOG_LEVEL_TRACE <= CURRENT_LOG_LEVEL) log_format_basic("TRACE", __VA_ARGS__); } while(0)

#endif // LOG_H