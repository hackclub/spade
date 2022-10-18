ls ../*.c ../*.h ../*.string | entr -s 'make && ./spade'

ls engine.js | entr -s './tools/cstringify.py engine.js > engine.js.cstring'
