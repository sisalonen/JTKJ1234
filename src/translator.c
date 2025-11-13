#include "translator.h"

// translator code source https://www.geeksforgeeks.org/cpp/program-for-morse-code-translator-conversion-of-morse-code-to-english-text/

char *morseToText(char morseCode[])
{
    // Morse code dictionary for A–Z
    const char *morseCodeDict[] = {
        ".-", "-...", "-.-.", "-..", ".", "..-.",
        "--.", "....", "..", ".---", "-.-", ".-..",
        "--", "-.", "---", ".--.", "--.-", ".-.",
        "...", "-", "..-", "...-", ".--", "-..-",
        "-.--", "--.."};

    static char result[1024]; // buffer for result
    int index = 0;
    result[0] = '\0';

    // Temporary copy since strtok modifies input
    static char temp[1024];
    strncpy(temp, morseCode, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *token = strtok(temp, " ");
    int spaceCount = 0;

    while (token != NULL)
    {
        // Count how many spaces were between this and the previous token
        char *next = strstr(morseCode + (token - temp) + strlen(token), " ");
        spaceCount = 0;
        while (next && *next == ' ')
        {
            spaceCount++;
            next++;
        }

        // Match Morse symbol to a letter
        int matched = 0;
        for (int i = 0; i < 26; i++)
        {
            if (strcmp(token, morseCodeDict[i]) == 0)
            {
                result[index++] = 'a' + i; // lowercase letters
                matched = 1;
                break;
            }
        }

        // If two or more spaces → new word
        if (spaceCount >= 2)
        {
            result[index++] = ' ';
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
