#include "translator.h"

// translator code source https://www.geeksforgeeks.org/cpp/program-for-morse-code-translator-conversion-of-morse-code-to-english-text/

char *morseToText(char morseCode[])
{
    // Morse code dictionary mapping to letters A-Z
    char *morseCodeDict[] = {
        ".-", "-...", "-.-.", "-..", ".", "..-.",
        "--.", "....", "..", ".---", "-.-", ".-..",
        "--", "-.", "---", ".--.", "--.-", ".-.",
        "...", "-", "..-", "...-", ".--", "-..-",
        "-.--", "--.."};

    // Static buffer to hold the translated text
    static char result[1024]; // 1024 is an arbitrary size; adjust as needed
    int index = 0;            // To keep track of the result string position

    // Tokenize the Morse code and translate each part
    char *token = strtok(morseCode, " ");
    while (token != NULL)
    {
        if (strcmp(token, "/") == 0)
        {
            result[index++] = ' '; // Space for "/"
        }
        else
        {
            // Try to match the Morse code to a letter
            for (int i = 0; i < 26; i++)
            {
                if (strcmp(token, morseCodeDict[i]) == 0)
                {
                    result[index++] = 'A' + i; // Translate to corresponding letter
                    break;
                }
            }
        }
        token = strtok(NULL, " ");
    }
    result[index] = '\0';

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
