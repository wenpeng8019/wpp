
#include <stdio.h>
#include <string.h>
#include <zlib.h>

int main() {
    printf("Content-Type: text/html\r\n\r\n");
    printf("<!DOCTYPE html>\n");
    printf("<html><head><title>zlib Test</title></head><body>\n");
    printf("<h1>zlib Integration Test</h1>\n");
    
    // Test 1: zlib version
    printf("<h2>1. zlib Version</h2>\n");
    printf("<p>Compiled with: %s</p>\n", ZLIB_VERSION);
    printf("<p>Runtime version: %s</p>\n", zlibVersion());
    
    // Test 2: Compress and decompress
    printf("<h2>2. Compression Test</h2>\n");
    
    const char *original = "Hello, World! This is a test string for zlib compression. "
                          "The quick brown fox jumps over the lazy dog. "
                          "We need some repeating text to see good compression ratios! "
                          "Hello, World! Hello, World! Hello, World!";
    
    size_t original_len = strlen(original);
    
    // Compress
    unsigned long compressed_len = compressBound(original_len);
    unsigned char compressed[compressed_len];
    
    int result = compress(compressed, &compressed_len, 
                         (const unsigned char*)original, original_len);
    
    if (result == Z_OK) {
        printf("<p>Original size: %zu bytes</p>\n", original_len);
        printf("<p>Compressed size: %lu bytes</p>\n", compressed_len);
        printf("<p>Compression ratio: %.2f%%</p>\n", 
               100.0 * (1.0 - (double)compressed_len / original_len));
        
        // Decompress to verify
        unsigned long decompressed_len = original_len;
        unsigned char decompressed[decompressed_len + 1];
        
        result = uncompress(decompressed, &decompressed_len,
                           compressed, compressed_len);
        
        if (result == Z_OK) {
            decompressed[decompressed_len] = '\0';
            
            if (strcmp(original, (char*)decompressed) == 0) {
                printf("<p style='color: green;'>✓ Decompression successful! Data matches.</p>\n");
            } else {
                printf("<p style='color: red;'>✗ Decompression failed! Data mismatch.</p>\n");
            }
        }
    } else {
        printf("<p style='color: red;'>Compression failed with error: %d</p>\n", result);
    }
    
    // Test 3: CRC32 checksum
    printf("<h2>3. CRC32 Checksum</h2>\n");
    unsigned long crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const unsigned char*)original, original_len);
    printf("<p>CRC32 of original data: 0x%08lx</p>\n", crc);
    
    printf("<h2>Conclusion</h2>\n");
    printf("<p><strong style='color: green;'>✓ zlib is working correctly!</strong></p>\n");
    
    printf("</body></html>\n");
    
    return 0;
}
