.global _start

.section .text

_start:
	// Here your execution starts
	
	ldr r0, =input // Finding the input
	mov r1, #1 // Resetting the value for the loop
	
	bl length_loop
	
	b check_palindrome
	
	b exit
	

length_loop: // Loop to find the length of the string 
	ldrb r2, [r0, r1]
	cmp r2, #0
	add r1, #1
	bne length_loop
	
	sub r1, #1
	mov r0, r1 // Returning the counted number - 1 as this is the length
	
	mov r15, r14 // Returning to the previous function 


	
check_palindrome:
	
	// We need to go through the input from back to front and check whether they are equal or not
	
	mov r4, r0 // Storing the length
	
	ldr r6, =input
	mov r7, #0 // This is the counter that goes from the front of the word
	mov r8, #0 // This is the counter that goes from the back of the word
	
	b check_palindrome_loop

	
	
	
	
	
check_palindrome_loop:

	sub r9, r4, r8 // Finding the offset for the second character
	sub r9, #1
	
	// Now we compare if we've gone through the word. The way we do this is by checking if 
	// the two indices have met. If this is the case then we have a palindrome!
	cmp r9, r7
	ble palindrome_found
	
	// We need to load the characters: 
	
	ldrb r10, [r6, r7] // Loading the first character we're checking
	ldrb r11, [r6, r9] // Loading the second character we're checking
	
	// Check whether the first character is a space
	cmp r10, #0x20
	beq first_character_space
	
	// Check whether the second character is a space
	cmp r11, #0x20
	beq second_character_space
	
	// If neither character is a space then we can go on to compare them, but first we have to check if they are uppercase
	
	// Check if any of the letters are uppercase
	mov r0, r10
	bl check_character_uppercase
	mov r10, r0
	
	mov r0, r11
	bl check_character_uppercase
	mov r11, r0
	
	// Now we've accounted for spaces and whether they are upper- or lowercase
	// This means we're ready to compare the two!
	
	cmp r10, r11 // If the two characters are not equal then it's not a palindrome
	bne palindrome_not_found
	
	// Now we increment the counters and iterate again!
	add r7, #1
	add r8, #1
	b check_palindrome_loop
	
// If the first character is on a space then we simply skip this character
first_character_space:
	add r7, #1
	b check_palindrome_loop

// If the second character is on a space then we simply skip this character 
second_character_space: 
	add r8, #1
	b check_palindrome_loop




// Checking of the character in r0 is uppercase. In which case, make it lowercase
check_character_uppercase:
	cmp r0, #0x60
	ble is_uppercase
	mov r15, r14
	
is_uppercase: 
	add r0, #0x20 // Making it lowercase
	mov r15, r14


palindrome_found:
	// Switch on only the 5 rightmost LEDs
	ldr r0, =0b0000011111
	ldr r1, =0xff200000
	str r0, [r1]
	
	
	
	
	// Write 'Palindrome detected' to UART
	
	ldr r0, =palindrome_detected
	ldr r1, =0xff201000
	mov r2, #0
	
	b write_to_uart

	
write_to_uart:
	ldrb r3, [r0, r2]
	cmp r3, #0
	beq exit
	
	str r3, [r1]
	add r2, #1
	b write_to_uart
	
	
palindrome_not_found:
	// Switch on only the 5 leftmost LEDs
	ldr r0, =0b1111100000
	ldr r1, =0xff200000
	str r0, [r1]
	
	// Write 'Not a palindrome' to UART
	ldr r0, =not_a_palindrome
	ldr r1, =0xff201000
	mov r2, #0
	
	b write_to_uart

	
	
exit:
	// Branch here for exit
	b exit
	

.section .data
.align
	// This is the input you are supposed to check for a palindrome
	// You can modify the string during development, however you
	// are not allowed to change the label 'input'!
	// input: .asciz "level"
	// input: .asciz "8448"
    // input: .asciz "KayAk"
    // input: .asciz "step on no pets"
    // input: .asciz "Never odd or even"
	// input: .asciz "palindrome"
	input: .asciz "1LevEL1"
	
	palindrome_detected: .asciz "Palindrome detected\n"
	not_a_palindrome: .asciz "Not a palindrome\n"


.end
