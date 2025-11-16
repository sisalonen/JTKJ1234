#pragma once

// translator code source https://www.geeksforgeeks.org/cpp/program-for-morse-code-translator-conversion-of-morse-code-to-english-text/

/* Return pointer to a static buffer containing translated text. Caller must
    copy if it needs to retain the string across further calls. */
char *morseToText(const char *morse);
