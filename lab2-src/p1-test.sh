#!/bin/bash

PSWD_FILE="./temp_pswd.txt"
SCRIPT_FILE="./pwcheck.sh"
PROMPT="Password Score:"

declare -a tests=(
	"qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq" 
	"abcd5" 
	"aBcDeF" 
	"aBcDeFgHiJkLmNoPqRsTuVwXyZaBcDe" 
	"aBc9F1" 
	"aBcDeFg94JkLmNo8qRsTuVwXyZaBcD2" 
	"aBcDeF@" 
	"aBcDeF$" 
	"AbCdEfGG" 
	"aBcDeFgg" 
	"aBcDeFgHiJkawfa" 
	"abcdefghijkl" 
	"AbCdEfGhIjKAWFA" 
	"ABCDEFGHIJKL" 
	"123456" 
	"ab12345f2456gD986f2e35a6f" 
	"Exon#Mobi@Le21" 
	"123456789abcdef@gDWSS@Aw4")

declare -a results=(
	"Error: Password length invalid."
	"Error: Password length invalid."
	"11"
	"36"
	"16"
	"41"
	"17"
	"17"
	"3"
	"3"
	"17"
	"14"
	"17"
	"14"
	"8"
	"32"
	"26"
	"21"
	)

function checkRes() {
	res=$1

	echo "testing ${tests[$i-1]}"

	if [ "$res" = "$PROMPT ${results[$i-1]}" ]
	then
		echo "passed"
	else
		echo "failed"
		echo "expected: $PROMPT ${results[$i-1]}"
		echo "found: $res"	
	fi

	echo "---------------------"
}

for (( i=1; i<${#tests[@]}+1; i++ ))
do
	echo "${tests[$i-1]}" > $PSWD_FILE
	res=$($SCRIPT_FILE $PSWD_FILE)

	checkRes "$res"
done