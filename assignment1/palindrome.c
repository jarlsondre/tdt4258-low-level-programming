#include <stdio.h>

unsigned int string_length(char str[]) {
    unsigned int length;
    for (length = 0; str[length] != 0; length++);

    return length; 
}

char* fix_string(char str[]) {

    unsigned int length = string_length(str); 


    static char output[1000]; 
    int index = 0; 

    for (int i = 0; i < length; i++) {
        // If it's a lowercase letter or a number we simply add it
        if ((str[i] >= 97 && str[i] <= 122) || (str[i] >= 48 && str[i] <= 57)) {
            output[index++] = str[i];
        }
        // If it's an uppercase letter then we add the corresponding lowercase letter 
        else if (str[i] >= 65 && str[i] <= 90) {
            output[index++] = str[i] + 32; 
        }

    }
    output[index] = '\0'; 
    return output; 
}

int palindrome_checker(char str[]) {

    str = fix_string(str); 
    unsigned int length = string_length(str); 
    for (int i = 0; i < length/2; i++) {
        if (str[i] != str[length - i - 1]) {
            return 0; 
        }
    }
    return 1; 
}

int main() {
    char test1[] = "aha"; 
    char test2[] = "hello";  
    char test3[] = "1991";  
    char test4[] = "Aahaa";  
    char test5[] = "A9a9a";    
    char test6[] = "ABBA";    
    char test7[] = "ABBAA";      
    char test8[] = "level";      
    char test9[] = "step on no pets";      
    char test10[] = "My gym";      
    char test11[] = "Was it a car or a cat I saw";      
    char test12[] = "Palindrome";      
    char test13[] = "First level";      


    printf("hello world\n "); 

    printf("is test1 a palindrome? %d \n", palindrome_checker(test1));
    printf("is test2 a palindrome? %d \n", palindrome_checker(test2));
    printf("is test3 a palindrome? %d \n", palindrome_checker(test3));
    printf("is test4 a palindrome? %d \n", palindrome_checker(test4));
    printf("is test5 a palindrome? %d \n", palindrome_checker(test5));
    printf("is test6 a palindrome? %d \n", palindrome_checker(test6));
    printf("is test7 a palindrome? %d \n", palindrome_checker(test7));
    printf("is test8 a palindrome? %d \n", palindrome_checker(test8));
    printf("is test9 a palindrome? %d \n", palindrome_checker(test9));
    printf("is test10 a palindrome? %d \n", palindrome_checker(test10));
    printf("is test11 a palindrome? %d \n", palindrome_checker(test11));
    printf("is test12 a palindrome? %d \n", palindrome_checker(test12));
    printf("is test13 a palindrome? %d \n", palindrome_checker(test13));


    return 0;
}
