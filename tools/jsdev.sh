ls engine.js game.js |
entr -s 'uglifyjs game.js -o game.min.js
	&& uglifyjs engine.js -o engine.min.js
	&& ./tools/cstringify.py engine.min.js > engine.min.js.cstring'