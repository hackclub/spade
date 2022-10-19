let setTimeout, setInterval, clearInterval, clearTimeout;
const {
  /* sprite interactions */ setSolids, setPushables,
  /*              see also: sprite.x +=, sprite.y += */

  /* art */ setLegend, setBackground,
  /* text */ addText, clearText,

  /*   spawn sprites */ setMap, addSprite,
  /* despawn sprites */ clearTile, /* sprite.remove() */

  /* tile queries */ getGrid, getTile, getFirst, getAll, tilesWith,
  /* see also: sprite.type */

  /* map dimensions */ width, height,

  /* constructors */ bitmap, tune, map,

  /* input handling */ onInput, afterInput,

  /* how much sprite has moved since last onInput: sprite.dx, sprite.dy */

  playTune,
} = (() => {
const exports = {};
/* re-exports from C; bottom of module_native.c has notes about why these are in C */
exports.setMap = map => native.setMap(map.trim());
exports.addSprite = native.addSprite;
exports.getGrid = native.getGrid;
exports.getTile = native.getTile;
exports.tilesWith = native.tilesWith;
exports.clearTile = native.clearTile;
exports.getFirst = native.getFirst;
exports.getAll = native.getAll;
exports.width = native.width;
exports.height = native.height;
exports.setBackground = native.setBackground;


/* opts: x, y, color (all optional) */
exports.addText = (str, opts={}) => {
  console.log("engine.js:addText");
  const CHARS_MAX_X = 21;
  const padLeft = Math.floor((CHARS_MAX_X - str.length)/2);

  native.text_add(
    str,
    opts.color ?? [10, 10, 40],
    opts.x ?? padLeft,
    opts.y ?? 0
  );
}

exports.clearText = () => native.text_clear();


exports.setLegend = (...bitmaps) => {
  console.log("engine.js:setLegend");
  native.legend_clear();
  for (const [charStr, bitmap] of bitmaps) {
    native.legend_doodle_set(charStr, bitmap.trim());
  }
  native.legend_prepare();
};

exports.setSolids = solids => {
  console.log("engine.js:setSolids");
  native.solids_clear();
  solids.forEach(native.solids_push);
};

exports.setPushables = pushTable => {
  console.log("engine.js:setPushables");
  native.push_table_clear();
  for (const [pusher, pushesList] of Object.entries(pushTable))
    for (const pushes of pushesList)
      native.push_table_set(pusher, pushes);
};

let afterInputs = [];
exports.afterInput = fn => (console.log('engine.js:afterInputs'), afterInputs.push(fn));

const button = {
  pinToHandlers: {
     "5": [],
     "7": [],
     "6": [],
     "8": [],
    "12": [],
    "14": [],
    "13": [],
    "15": [],
  },
  keyToPin: {
    "w":  "5",
    "s":  "7",
    "a":  "6",
    "d":  "8",
    "i": "12",
    "k": "14",
    "j": "13",
    "l": "15",
  }
};

native.press_cb(pin => {
  button.pinToHandlers[pin].forEach(f => f());

  afterInputs.forEach(f => f());

  native.map_clear_deltas();
});

{
  let timers = [];
  let id = 0;
  setTimeout  = (fn, ms) => (timers.push({ fn, ms, id }), id++);
  setInterval = (fn, ms) => (timers.push({ fn, ms, id, restartAt: ms }), id++);
  clearTimeout = clearInterval = id => {
    timers = timers.filter(t => t.id != id);
  };
  native.frame_cb(dt_secs => {
    const dt = dt_secs * 1000;
    timers = timers.filter(tim => {
      if (tim.ms <= 0) {
        /* trigger their callback */
        tim.fn();

        /* in case they cleared themselves */
        if (!timers.some(t => t == tim))
          return false;

        /* restart intervals, clear timeouts */
        if (tim.restartAt !== undefined)
          tim.ms = tim.restartAt;
        else
          return false;
      }
      tim.ms -= dt;
      return true;
    });
  });
}

exports.onInput = (key, fn) => {
  console.log("engine.js:onInput");
  const pin = button.keyToPin[key];

  if (pin === undefined)
    throw new Error(`the sprig doesn't have a "${key}" button!`);

  button.pinToHandlers[pin].push(fn);
};

exports.playTune = () => {};

function _makeTag(cb) {
  return (strings, ...interps) => {
    if (typeof strings === "string") {
      throw new Error("Tagged template literal must be used like name`text`, instead of name(`text`)");
    }
    const string = strings.reduce((p, c, i) => p + c + (interps[i] ?? ''), '');
    return cb(string);
  }
}
exports.bitmap = _makeTag(text => text);
exports.tune = _makeTag(text => text);
exports.map = _makeTag(text => text);
return exports;
})();
/*
@title: kill pigs
@author: luiq

Instructions:
  You are a storm god and you want some pigs to make your dinner...

  So you need to choose and kill the rigth pig into each of 4 levels
  Which level the number of pigs increase
  With "a" you move left
  With "d" you move rigth
  With "j" you throw the ray

  Enjoy and good dinner :)
*/

const player = "p"
const ray = "r"
const cloud1 = "c"
const cloud2 = "q"
const pig = "g"
const star = "s"

setLegend(
  [ player, bitmap`
.........6......
....0...06......
....0000066.....
.....000.666....
..0..000.666....
..00..0...666...
..000000000666..
..0000000000666.
.....000....666.
.....000.....66.
.....000.....66.
....00000.....6.
....00.00.....6.
....00.00.....6.
....00.00.......
....00.00.......`],
  [ ray, bitmap`
................
................
................
......6.........
......6.........
......66........
......666.......
.......66.......
.......66.......
........66......
.........6......
.........6......
................
................
................
................`],
  [ cloud1, bitmap`
....77.77.777...
7777777777777777
7777777777777777
77777777777777..
.7777777777777..
...7777777777...
....7777777.....
......77........
................
................
................
................
................
................
................
................`],
  [ cloud2, bitmap`
..77............
..777777.....77.
7777777.....7777
7777777777.77777
7777777777777777
.77777777777777.
.77777777777777.
....77.7777.....
................
................
................
................
................
................
................
................`],
  [ pig, bitmap`
................
...000.....000..
..0888.000.8880.
..0880088800880.
...00888888800..
...08028882080..
..0880088800880.
..0888800088880.
..0888088808880.
..0888008008880.
...08808880880..
...08880008880..
....088888880...
....080888080...
....088000880...
....000...000...`],
  [ star, bitmap`
5525555555555555
5555555555555555
5555555555555555
5555555555555555
5555555555555555
5555555555555555
5555555555555555
5555555555555555
5555555555552555
5555555555522255
5555555555222225
5555555555522255
5525555555552555
5222555555555555
2222255555555555
5222555555555555`],
);

let level = 0;
const levels = [
  map`
...p...
qcqcqcq
.......
.......
.......
..g.g..`,
  map`
...p...
qcqcqcq
.......
.......
.......
..ggg..`,
  map`
...p...
qcqcqcq
.......
.......
.......
g.ggg.g`,
  map`
...p...
qcqcqcq
.......
.......
.......
ggggggg`,
];

setBackground(star)
setMap(levels[level])
setSolids([ player, pig ]);
setPushables({
  [player]: []
});

let isThrowingRay = false
const timeout = (t) => new Promise((res) => setTimeout(() => res(), t))
const getRandomValueFromSize = (s) => Math.floor(Math.random()*s)
const setKeyMap = () => {
  const charMap = {
    a: (p) => () => p.x--,
    d: (p) => () => p.x++,
  }
  
  Object.keys(charMap).forEach((c) => 
    onInput(c, charMap[c](getFirst(player)))
  )
}
const startLevel = (level) => {
  setKeyMap()
  addText("" + (level + 1), { x: 1, y: 0, color: [ 255, 0, 0 ] })
  
}
startLevel(level)

const throwRay = async (x, y) => {
  isThrowingRay = true

  addSprite(x, y, ray)
  const thisRay = getFirst(ray)
  
  for (let i = 0; i < 5; i++) {
    await timeout(500)  
    thisRay.y++
  }
  isThrowingRay = false
  return thisRay.x
}

onInput("j", async () => {
  if (isThrowingRay) return

  const pigs = getAll(pig)
  const anyPig = pigs[getRandomValueFromSize(pigs.length)]
  pigs.forEach(({x, y}) => x !== anyPig.x ? clearTile(x, y) : false)
  
  const thisPlayer = getFirst(player)
  const rayPosition = await throwRay(thisPlayer.x, thisPlayer.y)

  const isWin = getAll(pig).some(({ x }) => x === rayPosition )
  const message = isWin ? "You Win" : "You Lose"
  
  addText(message, { x: 6, y: 8, color: [ 255, 0, 0 ] })
  await timeout(1000)
  clearText()
  
  if (isWin) {
    ++level
    
    if (!levels[level]) {
      level = 0
    }
    
    setMap(levels[level])
    startLevel(level)

    return
  }

  setMap(levels[level])
  startLevel(level)
});
