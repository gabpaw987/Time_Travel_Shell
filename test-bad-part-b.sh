#! /bin/sh

# UCLA CS 111 Lab 1 - Test that syntax errors are caught.

tmp=$0-$$.tmp
mkdir "$tmp" || exit
(
cd "$tmp" || exit
status=

n=1
for bad in \
  'cat non-existing.file' \
  'cat < non-existing.file' \
  'non-existing-command -x > file' \
  'echo "unallowed characters"' \
  'echo empty output  >' \
  'rm tmp.txt ; echo wrong order > tmp.txt' \
  'non-existing command | echo in first place of sequence' \
  'echo test ; (invalid subshell command) > tmp.txt' \
  'rm tmp.txt ; echo error in second command of pipe | rm non-existing.file'
do
  echo "$bad" >test$n.sh || exit
  ../timetrash test$n.sh >test$n.out 2>test$n.err
  test -s test$n.err || {
    echo >&2 "test$n: no error message for: $bad"
    status=1
  }
  n=$((n+1))
done

exit $status
) || exit

rm -fr "$tmp"
