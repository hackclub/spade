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

  /* constructors */ bitmap, tune, map, color,

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
  // console.log("engine.js:addText");
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
  // console.log("engine.js:setLegend");
  native.legend_clear();
  for (const [charStr, bitmap] of bitmaps) {
    native.legend_doodle_set(charStr, bitmap.trim());
  }
  native.legend_prepare();
};

exports.setSolids = solids => {
  // console.log("engine.js:setSolids");
  native.solids_clear();
  solids.forEach(native.solids_push);
};

exports.setPushables = pushTable => {
  // console.log("engine.js:setPushables");
  native.push_table_clear();
  for (const [pusher, pushesList] of Object.entries(pushTable))
    for (const pushes of pushesList)
      native.push_table_set(pusher, pushes);
};

let afterInputs = [];
// exports.afterInput = fn => (console.log('engine.js:afterInputs'), afterInputs.push(fn));
exports.afterInput = fn => afterInputs.push(fn);

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
  const timers = [];
  let id = 0;
  setTimeout  = (fn, ms) => (timers.push({ fn, ms, id }), id++);
  setInterval = (fn, ms) => (timers.push({ fn, ms, id, restartAt: ms }), id++);
  clearTimeout = clearInterval = id => {
    timers.splice(timers.findIndex(t => t.id == id), 1);
  };
  native.frame_cb(dt => {
    const errors = [];

    for (const tim of [...timers]) {
      if (!timers.includes(tim)) continue; /* just in case :) */

      if (tim.ms <= 0) {
        /* trigger their callback */
        try {
          tim.fn();
        } catch (error) {
          if (error) errors.push(error);
        }

        /* restart intervals, clear timeouts */
        if (tim.restartAt !== undefined)
          tim.ms = tim.restartAt;
        else
          timers.splice(timers.indexOf(tim), 1);
      }
      tim.ms -= dt;
    }

    /* we'll never need to throw more than one error -ced */
    if (errors.length > 0) throw errors[0];
  });
}

exports.onInput = (key, fn) => {
  // console.log("engine.js:onInput");
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
exports.color = _makeTag(text => text);

// .at polyfill
function at(n) {
	// ToInteger() abstract op
	n = Math.trunc(n) || 0;
	// Allow negative indexing from the end
	if (n < 0) n += this.length;
	// OOB access is guaranteed to return undefined
	if (n < 0 || n >= this.length) return undefined;
	// Otherwise, this is just normal property access
	return this[n];
}

const TypedArray = Reflect.getPrototypeOf(Int8Array);
for (const C of [Array, String, TypedArray]) {
    Object.defineProperty(C.prototype, "at",
                          { value: at,
                            writable: true,
                            enumerable: false,
                            configurable: true });
}

return exports;
})();
/*
@title: Zooter
@author: PerrinPerson
*/

const player = "p";
const line = "l";
const enemy = "e";
const proj = "o";
const bg = "b";
const gear = tune `
60.120240480961925,
60.120240480961925: e5-60.120240480961925 + d5^60.120240480961925,
60.120240480961925: e5^60.120240480961925 + d5/60.120240480961925,
60.120240480961925: e5/60.120240480961925 + d5-60.120240480961925,
1683.366733466934`;
const shoot = tune `
40.48582995951417: b5~40.48582995951417 + a5~40.48582995951417 + g5~40.48582995951417 + f5~40.48582995951417 + e5~40.48582995951417,
40.48582995951417: e5~40.48582995951417 + b5~40.48582995951417 + f5^40.48582995951417 + g5^40.48582995951417 + a5^40.48582995951417,
40.48582995951417: b5~40.48582995951417 + a5~40.48582995951417 + g5~40.48582995951417 + f5~40.48582995951417 + e5~40.48582995951417,
1174.089068825911`;
const die = tune `
40.48582995951417: d4^40.48582995951417 + c4-40.48582995951417 + e4^40.48582995951417 + f4-40.48582995951417,
40.48582995951417: f4-40.48582995951417 + e4^40.48582995951417 + d4^40.48582995951417 + c4-40.48582995951417,
1214.5748987854251`;
const touch = tune `
119.04761904761905: b5-119.04761904761905,
119.04761904761905: a5-119.04761904761905,
119.04761904761905: g5-119.04761904761905,
119.04761904761905: f5-119.04761904761905,
119.04761904761905: e5-119.04761904761905,
119.04761904761905: d5-119.04761904761905,
119.04761904761905: c5-119.04761904761905,
119.04761904761905: b4-119.04761904761905,
119.04761904761905: a4-119.04761904761905,
119.04761904761905: g4-119.04761904761905,
119.04761904761905: f4-119.04761904761905,
119.04761904761905: e4-119.04761904761905,
119.04761904761905: d4-119.04761904761905,
119.04761904761905: c4-119.04761904761905,
2142.857142857143`

let score = 0;
let projActive = false;
let timers = [];

setLegend(
  [ player, bitmap`
................
................
................
..00000000......
..0H77HHH00.....
..0H77HHHH0.....
..0C77CCCC0.....
..0C77CCCC000000
..0C77CCCC000000
..0C77CCCC0.....
..0H77HHHH0.....
..0H77HHH00.....
..00000000......
................
................
................`],
  [ line, bitmap`
....77..........
....77..........
....77..........
....77..........
....77..........
....77..........
....77..........
....77..........
....77..........
....77..........
....77..........
....77..........
....77..........
....77..........
....77..........
....77..........`],
  [ enemy, bitmap`
.........L.LL...
........L.L.L...
........LLLL....
........44444...
........40404...
........44444...
........46664...
........44444...
......LLLL55....
..........55....
......LLLL55....
..........55....
.........5555...
.........5..5...
.........5..5...
.........5..5...`],
  [ proj, bitmap`
................
................
................
................
................
................
................
......L0........
......0L0.......
......L0........
................
................
................
................
................
................`],
  [ bg, bitmap`
DDDDDDDDDDDDDDDD
DDDDD1DDDDD1DDDD
DDDDDDDDDDDDDDDD
DDDDDDDDDDDDDDDD
DDDDDDDDDDDDDD1D
DDD1DDDD1DDDDDDD
DDD1DDDDDDDDDDDD
DDDDDDDDDDDDDDDD
DDDDDDDDD1DDDDDD
1DDDDDD1DDDDDDDD
DDDDDDDDDDDDDDDD
DDDDDDDDDDDDDDDD
DDDDD1DDDDDDDDDD
DDD1DDDDDDDDDDDD
DDDDDDDDDD1DDDDD
DDDDDDDDDDDDDD1D`],
);

setSolids([]);

let level = 0;
const levels = [
  map`
l.......
l.......
l.......
l.......`,
];

setMap(levels[level]);
setBackground(bg);
addSprite(0, 0, player);

setPushables({
    [ player ]: [],
});

onInput("s", () => {
  playTune(gear)
  getFirst(player).y += 1;
});

onInput("w", () => {
  playTune(gear)
  getFirst(player).y -= 1;
});

onInput("k", () => {
  if (projActive === false) {
      playTune(shoot)
      let p = getFirst(player);
      addSprite(p.x + 1, p.y, proj);
      projActive = true;
  }
});

onInput("j", () => {
  timers.forEach((timer) => {
    clearInterval(timer);
  });

  getAll(proj).forEach((projObj) => {
    projObj.remove();
  });

  projActive = false;
  
  getAll(enemy).forEach((enemyObj) => {
    enemyObj.remove();
  });

  score = 0;

  clearText();
  
  startgame();  
});

function startgame() {
  // check for game over and draw score
  timers.push(setInterval(() => {
    addText("SCORE: " + score, {
      x: 5,
      y: 0,
      color: color `0`
    });
    let gameover = false;
    getAll(enemy).forEach((enemyObj) => {
      if (enemyObj.x === 0) {
        gameover = true;
        return;
      }
    });
  
    if (gameover) {
      getAll(enemy).forEach((enemyObj) => {
        if (enemyObj.x != 0) {
          enemyObj.remove();
        }
      });
      playTune(touch);
    
      addText("R.I.P", {
        x: 4,
        y: 6,
        color: color `5` 
      });
  
      timers.forEach((timer) => {
        clearInterval(timer);
      });
    }
  }, 50));
  
  // moves the enemies
  timers.push(setInterval(() => {
    getAll(enemy).forEach((enemyObj) => {
      enemyObj.x -= 1;
    });
  }, 600));
  
  //  adds the enemies
  timers.push(setInterval(() => {
    for (let i = 0; i < 4; i += 1) {
      let dospawn =  Math.random();
      if (dospawn <= 0.35) {
        addSprite(7, i, enemy);
      }
    }
  }, 800));
  
  // projectile movement
  timers.push(setInterval(() => {    
    if (projActive) {
      getFirst(proj).x += 1
    }
  }, 100));
  
  timers.push(setInterval(() => {
    if (projActive) {
      // projectile hit end of screen
      let p = getFirst(proj);
      if (p.x === 7) {
        p.remove();
        projActive = false;
      }
    }

    if (projActive) {
      // projectile hit enemy
      let p = getFirst(proj);
      getAll(enemy).forEach((enemyObj) => {
        if (p.x === enemyObj.x && p.y === enemyObj.y) {
          enemyObj.remove();
          playTune(die);
          p.remove();
          score += 1;
          projActive = false;
          return;
        }
      });
    }
  }, 20));
}

startgame();