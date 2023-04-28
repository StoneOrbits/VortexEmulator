#!/bin/bash

VALGRIND="valgrind --quiet --leak-check=full --show-leak-kinds=all"
VORTEX="../vortex"

echo -e -n "\e[33mBuilding Vortex...\e[0m"
make -C ../ &> /dev/null
if [ $? -ne 0 ]; then
  echo -e "\e[31mFailed to build Vortex!\e[0m"
  exit
fi
if [ ! -x "$VORTEX" ]; then
  echo -e "\e[31mCould not find Vortex!\e[0m"
  exit
fi
echo -e "\e[32mSuccess\e[0m"

rm -rf tmp
mkdir tmp

FILES=*.test
NUMFILES="$(echo $FILES | wc -w)"

echo -e "\e[33m== [\e[31mRECORDING \e[97m$NUMFILES INTEGRATION TESTS\e[33m] ==\e[0m"

for FILE in *.test; do
  INPUT="$(grep "Input=" $FILE | cut -d= -f2)"
  BRIEF="$(grep "Brief=" $FILE | cut -d= -f2)"
  echo -e -n "\e[31mRecording \e[33m[\e[97m$BRIEF\e[33m]...\e[0m"
  TEMP_FILE="tmp/${FILE}.out"
  # Truncate everything after and including the line with ""
  awk '/^[-]{80}$/{print; exit} 1' $FILE > $TEMP_FILE
  # Append the output of the $VORTEX command to the temp file
  # NOTE: When recording the tests we don't use valgrind because
  #       the valgrind output should be clean anyway. But when running
  #       the test valgrind is used with --leak-check=full --show-leak-kinds=all
  $VORTEX -t <<< $INPUT >> $TEMP_FILE
  # Replace the original file with the modified temp file
  mv $TEMP_FILE $FILE
  echo -e "\e[96mOK\e[0m"

  # print out colorful if in verbose
  if [ "$1" == "-v" ]; then
    $VORTEX -tc <<< $INPUT
  fi
done

rm -rf tmp
