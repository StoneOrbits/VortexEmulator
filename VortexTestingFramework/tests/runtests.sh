#!/bin/bash

VALGRIND="valgrind --quiet --leak-check=full --show-leak-kinds=all"
VORTEX="../vortex"

make -C ../ &> /dev/null
if [ $? -ne 0 ]; then
  echo "Failed to build Vortex!"
  exit
fi

if [ ! -x "$VORTEX" ]; then
  echo "Could not find $VORTEX"
  exit
fi

rm -rf tmp
mkdir tmp

ALLSUCCES=1

echo -e "\e[33m== [\e[97mVORTEX INTEGRATION TESTS\e[33m] ==\e[0m"

for FILE in *.test; do
  INPUT="$(grep "Input=" $FILE | cut -d= -f2)"
  BRIEF="$(grep "Brief=" $FILE | cut -d= -f2)"
  echo -e -n "\e[33mTesting [\e[97m$BRIEF\e[33m]... \e[0m"
  DIVIDER=$(grep -n "Initializing..." $FILE | cut -f1 -d:)
  EXPECTED="tmp/${FILE}.expected"
  OUTPUT="tmp/${FILE}.output"
  DIFF="tmp/${FILE}.diff"
  tail -n +$DIVIDER "$FILE" &> $EXPECTED
  $VALGRIND $VORTEX <<< $INPUT &> $OUTPUT
  diff -q $EXPECTED $OUTPUT &> $DIFF
  if [ $? -eq 0 ]; then
    echo -e "\e[32mSUCCESS\e[0m"
  else
    echo -e "\e[31mFAILURE\e[0m"
    ALLSUCCES=0
    break
  fi
done

# check if all test succeeded
if [ $ALLSUCCES -eq 1 ]; then
  echo -e "\e[33m== [\e[32mSUCCESS ALL TESTS PASSED\e[33m] ==\e[0m"
  # if so clear the tmp folder
  rm -rf tmp
else
  # otherwise cat the last diff
  diff $EXPECTED $OUTPUT
fi
