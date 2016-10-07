#! /bin/sh

# UCLA CS 111 Lab 1c Design

tmp=$0-$$.tmp
mkdir "$tmp" || exit

(
cd "$tmp" || exit

cat >test.sh <<'EOF'
echo x > a;
cp a b | echo x
echo a | rm a | echo c
(echo hello; echo many_commands
echo start here && echo it can) &&
echo execute; echo them all
EOF

cat >test.exp <<'EOF'
x
c
hello
many_commands
start here
it can
execute
them all
Insufficient max processes to run command 1 in parallel. The last 1 piped command(s) will be executed in sequence!
EOF

../timetrash -t -N 4 test.sh >test.out 2>test.err || exit

diff -u test.exp test.out || exit
test ! -s test.err || {
  cat test.err
  exit 1
}

) || exit

rm -fr "$tmp"
