#! /bin/sh

# UCLA CS 111 Lab 1 - Test that valid syntax is processed correctly.

tmp=$0-$$.tmp
mkdir "$tmp" || exit

(
cd "$tmp" || exit

cat >test.sh <<'EOF'
echo some input here > tmp.txt
(echo subshell input
 echo subshell input;
 echo line 3;) > ./tmp.txt | cat;

sort -u < tmp.txt > sorted.txt
rm tmp.txt | cat <sorted.txt

echo 1 | echo 2 | echo 3
(echo 1 ; echo 2) ; echo 3
(echo 1 && (echo 2 ; echo 3) || echo 4 ) | cat

echo break

#comment test
false && echo 1; true || echo 2;
(false || (true || echo 1) && echo 2) && (true || (echo 1 && echo 2));
EOF

cat >test.exp <<'EOF'
line 3
subshell input
3
1
2
3
1
2
3
break
2
EOF

../timetrash test.sh >test.out 2>test.err || exit

diff -u test.exp test.out || exit
test ! -s test.err || {
  cat test.err
  exit 1
}

) || exit

rm -fr "$tmp"
