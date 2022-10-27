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
  let timers = [];
  let id = 0;
  setTimeout  = (fn, ms) => (timers.push({ fn, ms, id }), id++);
  setInterval = (fn, ms) => (timers.push({ fn, ms, id, restartAt: ms }), id++);
  clearTimeout = clearInterval = id => {
    timers = timers.filter(t => t.id != id);
  };
  native.frame_cb(dt => {
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
return exports;
})();
/*
@title: Zombie Defense
@author: Edmund

Controls:
  W, S - Movement
  I - Shooting
  L - Reload
  J - Restart game

Goal:
  Get the highest score
*/

function range(start, stop, step) {
    if (typeof stop == 'undefined') {
        // one param defined
        stop = start;
        start = 0;
    }

    if (typeof step == 'undefined') {
        step = 1;
    }

    if ((step > 0 && start >= stop) || (step < 0 && start <= stop)) {
        return [];
    }

    var result = [];
    for (var i = start; step > 0 ? i < stop : i > stop; i += step) {
        result.push(i);
    }

    return result;
};

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

const player = "p";
const zombie = "z";
const barrier = "b";
const grass = "g";
const road = "r"
const background = "x";
const bullet = "t";
const textArea = "a";
const roadStripes = "s";

const shootSFX = tune`
49.504950495049506: d4/49.504950495049506,
1534.6534653465346`
const zombieAttackSFX = tune`
30: d4/30,
30: d4/30,
30: d4/30,
30: d4/30,
30: d4/30,
30: d4/30,
780`
const loseTune = tune`
151.5151515151515: g4/151.5151515151515,
151.5151515151515: f4/151.5151515151515,
151.5151515151515: e4/151.5151515151515,
151.5151515151515: d4/151.5151515151515,
151.5151515151515: c4/151.5151515151515,
151.5151515151515: c4/151.5151515151515,
151.5151515151515: c4/151.5151515151515,
3787.8787878787875`
const zombieDieSFX = tune`
47.24409448818898: b4-47.24409448818898,
47.24409448818898: b4-47.24409448818898,
47.24409448818898: b4-47.24409448818898,
1370.0787401574803`
const zombieMoveSFX = tune`
326.0869565217391: c4^326.0869565217391,
10108.695652173912`
const outOfBulletSFX = tune `
59.28853754940712: d5/59.28853754940712,
59.28853754940712: e5/59.28853754940712,
59.28853754940712: f5/59.28853754940712,
1719.3675889328065`

setLegend(
  [ textArea, bitmap`
1111111111111111
1111111111111111
1111111111111111
1111111111111111
1111111111111111
1111111111111111
1111111111111111
1111111111111111
1111111111111111
1111111111111111
1111111111111111
1111111111111111
1111111111111111
1111111111111111
1111111111111111
1111111111111111`],
  [ bullet, bitmap`
................
................
................
................
................
................
....666666......
....6666663.....
....6666663.....
....666666......
................
................
................
................
................
................`],
  [ player, bitmap`
................
......55........
.....5555.......
.....5565.......
.....2222.......
.....2020.......
.....2222.......
......22.....0..
.....55555..LLLL
....55577777L.1.
...55777555.L1..
...555557777L...
...55555555.....
...55555555.....
...55555555.....
....55..55......`],
  [ zombie, bitmap`
.......4444.....
.....4444444....
....444444444...
....433443344...
...44334433444..
...44444444444..
5..44444444444..
55554L2L2L2L44..
...44L2L2L2L444.
...44LLLLLLL444.
...44LL2L2L4455.
...44442L244554.
.....44...5554..
.....44.555444..
...4444..44444..
...4444..44444..`],
  [ barrier, bitmap`
......LLL.......
.....11111......
.....11111......
......LLL.......
.....11111......
.....11111......
......LLL.......
.....11111......
.....11111......
......LLL.......
.....11111......
.....11111......
......LLL.......
.....11111......
.....11111......
......LLL.......`],
  [ grass, bitmap`
4444444444444444
4440044444444444
4400004444004444
4004404440004444
4444444440400444
4444444440440444
4440044444444444
4400044444444444
4404004444444444
4444444444444444
4444444444400444
4444004444004044
4440004440444044
4440404444444444
4444444444444444
4444444444444444`],
  [ road, bitmap`
0000000000000000
0000000000000000
0000000000000000
0000000000000000
0000000000000000
0000000000000000
0000000000000000
0000000000000000
0000000000000000
0000000000000000
0000000000000000
0000000000000000
0000000000000000
0000000000000000
0000000000000000
0000000000000000`],
  [ background, bitmap`
4444444444444444
4444444444444444
4444444444444444
4444444444444444
4444444444444444
4444444444444444
4444444444444444
4444444444444444
4444444444444444
4444444444444444
4444444444444444
4444444444444444
4444444444444444
4444444444444444
4444444444444444
4444444444444444`],
  [ roadStripes, bitmap`
0000000000000000
0000000000000000
0000000000000000
0000000000000000
0000000000000000
0000000000000000
2222222222222222
2222222222222222
2222222222222222
2222222222222222
0000000000000000
0000000000000000
0000000000000000
0000000000000000
0000000000000000
0000000000000000`],
);

setSolids([player, barrier, textArea]);
setBackground(background)

let level = 0;
const levels = [
  map`
aaaaaaaaaaaaaaaaa
aaaaaaaaaaaaaaaaa
.bggggggggggggggg
.brrrrrrrrrrrrrrr
.brrrrrrrrrrrrrrr
.brrrrrrrrrrrrrrr
pbrssrssrssrssrss
.brrrrrrrrrrrrrrr
.brrrrrrrrrrrrrrr
.brrrrrrrrrrrrrrr
.bggggggggggggggg
aaaaaaaaaaaaaaaaa
aaaaaaaaaaaaaaaaa`,
];

setMap(levels[level]);

// Initialization stuff done

// Changeable Values
const spawnTilesX = 16
const spawnTilesY = range(3, 10)
const initBullets = 5;
const initHealth = 10;
const initReloadTimeSeconds = 3;
const initZombieWalkSpeed = 2000;
const initZombieSpawnRate = 0.1;

// Internal values
let bullets = initBullets;
let score = 0;
let health = initHealth;
let gameOver = false;
let reloadTimeSeconds = initReloadTimeSeconds;
let isReloading = false;
let reloadRemainTime = 0;
let zombieWalkSpeed = initZombieWalkSpeed;
let zombieSpawnRate = initZombieSpawnRate;

onInput("w", () => {
  if (gameOver) return;
  getFirst(player).y -= 1;
});

onInput("s", () => {
  if (gameOver) return;
  getFirst(player).y += 1;
});

onInput("i", () => {
  if (gameOver) return;
  if (bullets <= 0) {
    playTune(outOfBulletSFX);
    return;
  }
  playTune(shootSFX);
  const playerObj = getFirst(player);
  bullets--
  addSprite(playerObj.x + 1, playerObj.y, bullet);
  if (bullets <= 0) {
    playTune(outOfBulletSFX);
  }
});

onInput("j", () => {
  restartGame();
});

onInput("l", () => {
  // Checking for bullet state
  if (bullets != 5) {
    reload()
  }
});

const reload = async () => {
  if (isReloading) return;
  addText(`Reloading...`, {
    x: 1, 
    y: 14, 
    color: color`7`
  })
  isReloading = true
  for (let i = reloadTimeSeconds; i > 0; i--) {
        reloadRemainTime = i
        await sleep(1000);
    }
  bullets = 5
  isReloading = false
  await sleep(3000);
}

const restartGame = async () => {
  // Reinit game internal values
  bullets = initBullets;
  score = 0;
  health = initHealth;
  gameOver = false;
  reloadTimeSeconds = initReloadTimeSeconds;
  isReloading = false;
  reloadRemainTime = 0;
  zombieWalkSpeed = initZombieWalkSpeed;
  zombieSpawnRate = initZombieSpawnRate;

  // Remove all zombies
  getAll(zombie).forEach((zombie) => {
    zombie.remove();
  });
}

setInterval(() => {
  if (gameOver) return;
  clearText()
  if (!isReloading) {
    if (bullets > 0) {
      addText(`Bullets:${bullets}`, {
        x: 1, 
        y: 14, 
        color: color`4`
      })
    } else {
      addText(`Reload!`, {
        x: 1, 
        y: 14, 
        color: color`3`
      })
    }
  } else {
    addText(`Reloading...${reloadRemainTime}`, {
      x: 1, 
      y: 14, 
      color: color`7`
    })
  }
  addText(`Score:${score}`, {
    x: 1, 
    y: 1, 
    color: color`5`
  })
  addText(`Health:${health}`, {
    x: 10, 
    y: 1, 
    color: color`3`
  })
}, 10)
 
setInterval(() => {
  getAll(bullet).forEach((bulletObj) => {
    getTile(bulletObj.x + 1, bulletObj.y).forEach((sprite) => {
      if (sprite.type === zombie) {
        // Zombie detected
        playTune(zombieDieSFX);
        sprite.remove()
        bulletObj.remove()
        score += 1
        return;
      }
    })
    if (bulletObj.x === 16) {
      bulletObj.remove();
    }
    bulletObj.x += 1
  }) 
}, 30)

setInterval(() => {
  if (health > 0) return;
  if (gameOver) return;
  gameOver = true;
  clearText()
  addText(`Game over`, {
    x: 6, 
    y: 6, 
    color: color`3`
  })
  addText(`Score:${score}`, {
    x: 7, 
    y: 7, 
    color: color`3`
  })
  addText(`Press j to retry`, {
    x: 3, 
    y: 8, 
    color: color`3`
  })
  // Remove all zombies
  getAll(zombie).forEach((zombie) => {
    zombie.remove();
  });
  playTune(loseTune);
}, 30)

setInterval(() => {
  if (gameOver) return;
  for (let y = spawnTilesY[0]; y < spawnTilesY.slice(-1)[0]; y++) {
    if (Math.random() < zombieSpawnRate) {
      addSprite(spawnTilesX, y, zombie);
    }
  }
}, zombieWalkSpeed)

setInterval(() => {
  if (gameOver) return;
  if (getFirst(zombie)) {
    playTune(zombieMoveSFX);
  };
  getAll(zombie).forEach((zombieObj) => {
    if (zombieObj.x === 2) {
      // Touched gate
      playTune(zombieAttackSFX);
      health--
      zombieObj.remove();
    }
    zombieObj.x -= 1
  }) 
}, zombieWalkSpeed)

// Game gets harder every 5 seconds
setInterval(() => {
  if (zombieWalkSpeed > 1000) {
    
  }
  zombieWalkSpeed -= 250
  if (zombieSpawnRate < 0.4) {
    zombieSpawnRate += 0.05
  }
}, 5 * 1000)
