#!/bin/bash

# Variables
patterns=(0)
colorsets=("red" "red,green" "red,green,blue")
max_args=5
max_value=3
create_test_script_path="./create_test.sh -n"  # Update the path to your create_test.sh script if needed

# Function to generate all combinations of arguments
generate_args() {
    local current_arg=$1
    local current_str=$2

    if [[ $current_arg -le $max_args ]]; then
        for ((i=0; i<=$max_value; i++)); do
            generate_args $((current_arg+1)) "${current_str}${i},"
        done
    else
        echo "${current_str%,}"
    fi
}

# Generate tests
for pattern in ${patterns[@]}; do
    for colorset in ${colorsets[@]}; do
        for arg_str in $(generate_args 1 ""); do
            # Create the command string
            command_string="-P${pattern} -C${colorset} -A${arg_str}"

            # Create the test name
            test_name="Pattern_${pattern}_Colorset_${colorset//,/}_Args_${arg_str//,/}"

            # Create the test description
            test_descr="Test for pattern ${pattern}, colorset ${colorset} and arguments ${arg_str}"

            # Create the test
            echo -e "1\n${command_string}\nw100q\n${test_name}\n${test_descr}\n" | $create_test_script_path
        done
    done
done

