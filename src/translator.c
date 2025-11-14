#include "translator.h"

// translator code source https://www.geeksforgeeks.org/cpp/program-for-morse-code-translator-conversion-of-morse-code-to-english-text/
char *morseToText(const char *morse)
{
    // Morse dictionary (A–Z)
    static const char *dict[] = {
        ".-", "-...", "-.-.", "-..", ".", "..-.",
        "--.", "....", "..", ".---", "-.-", ".-..",
        "--", "-.", "---", ".--.", "--.-", ".-.",
        "...", "-", "..-", "...-", ".--", "-..-",
        "-.--", "--.."};

    static char result[1024];
    int r = 0;

    char buffer[16];
    int b = 0;

    int spaceCount = 0;

    for (int i = 0;; i++)
    {
        char c = morse[i];

        if (c == '.' || c == '-')
        {
            buffer[b++] = c;
            spaceCount = 0;
        }
        else if (c == ' ' || c == '\0')
        {
            if (b > 0)
            {
                buffer[b] = '\0';

                for (int k = 0; k < 26; k++)
                {
                    if (strcmp(buffer, dict[k]) == 0)
                    {
                        result[r++] = 'a' + k;
                        break;
                    }
                }

                b = 0;
            }

            spaceCount++;

            if (spaceCount == 3)
            {
                result[r++] = ' ';
            }

            if (c == '\0')
                break;
        }
    }

    result[r] = '\0';
    return result;
}
// // Example Morse code
// char morseCode[] = "-- --- .-. ... . / -.-. --- -.. . / .. ... / "
//                    "..-. --- .-. --. . - - .- -... .-.. .";

// printf("Morse Code: %s\n", morseCode);

// // Get the translated text
// char* translatedText = morseToText(morseCode);
// printf("English Text: %s\n", translatedText);

// return 0;
