#!/bin/sh

set -x

ext_lists="c cpp h hpp"
format_include_file=.format_include
format_exclude_file=.format_exclude
format_cmd="clang-format -i -style='{ BasedOnStyle: LLVM, BreakBeforeBraces: Linux, UseTab: Never, IndentWidth: 4, TabWidth: 4, ColumnLimit: 100, SortIncludes: false}'"

inc_list=`cat $format_include_file`
exc_list=`cat $format_exclude_file`


ext_arg_list=""
for i in $ext_lists
do
	ext_arg_list="$ext_arg_list -o -name \"*.$i\""
done


prune_arg_lists=""
for i in $exc_list
do
	if [ "$prune_arg_lists" != "" ]; then
		prune_arg_lists="$prune_arg_lists -o -path \"${i}*\" -prune"
	else
		prune_arg_lists="-path \"${i}*\" -prune"
	fi

done


for i in $inc_list
do
	cmd=`echo "find $i $prune_arg_lists $ext_arg_list | xargs $format_cmd " > /tmp/format_code.sh`
	sh /tmp/format_code.sh
done


