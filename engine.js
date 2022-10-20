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
@title: tetris
@author: neesh
*/

const rows = 12;
const cols = 8;
const borders = false;
const emptyColor = "0";

const iPiece = [
      [  true,  true,  true,  true ]
  ]
const jPiece = [
      [  true, false, false ],
      [  true,  true,  true ]
]
const lPiece = [
    [ false, false,  true ],
    [  true,  true,  true ]
]
const oPiece = [
    [  true,  true ],
    [  true,  true ]
]
const sPiece = [
    [ false,  true,  true ],
    [  true,  true, false ]
]
const tPiece = [
    [ false,  true, false ],
    [  true,  true,  true ]
]
const zPiece = [
    [  true,  true, false ],
    [ false,  true,  true ]
]
let board = [];
let score = 0;
const music = tune`
297.029702970297: e5-297.029702970297 + c4~297.029702970297,
297.029702970297: d4~297.029702970297,
297.029702970297: b4-297.029702970297 + g4~297.029702970297 + c4~297.029702970297,
297.029702970297: c5-297.029702970297 + a4~297.029702970297 + d4~297.029702970297,
297.029702970297: d5-297.029702970297 + b4~297.029702970297 + c4~297.029702970297,
297.029702970297: d4~297.029702970297,
297.029702970297: c5-297.029702970297 + c4~297.029702970297,
297.029702970297: b4-297.029702970297 + d4~297.029702970297,
297.029702970297: a4-297.029702970297 + c4~297.029702970297,
297.029702970297: d4~297.029702970297,
297.029702970297: a4-297.029702970297 + f4~297.029702970297 + c4~297.029702970297,
297.029702970297: c5-297.029702970297 + a4~297.029702970297 + d4~297.029702970297,
297.029702970297: e5-297.029702970297 + c5~297.029702970297 + c4~297.029702970297,
297.029702970297: d4~297.029702970297,
297.029702970297: d5-297.029702970297 + c4~297.029702970297,
297.029702970297: c5-297.029702970297 + d4~297.029702970297,
297.029702970297: b4-297.029702970297 + g4~297.029702970297 + c4~297.029702970297,
297.029702970297: d4~297.029702970297,
297.029702970297: c5-297.029702970297 + c4~297.029702970297 + a4~297.029702970297,
297.029702970297: d5-297.029702970297 + d4~297.029702970297 + b4~297.029702970297,
297.029702970297: c4~297.029702970297 + c5~297.029702970297 + e5-297.029702970297,
297.029702970297: d4~297.029702970297,
297.029702970297: d5-297.029702970297 + c4~297.029702970297,
297.029702970297: d4~297.029702970297,
297.029702970297: a4-297.029702970297 + f4~297.029702970297 + c4~297.029702970297,
297.029702970297: d4~297.029702970297,
297.029702970297: a4-297.029702970297 + f4~297.029702970297 + c4~297.029702970297,
297.029702970297: d4~297.029702970297,
297.029702970297: c4~297.029702970297,
297.029702970297: d4~297.029702970297,
297.029702970297: c4~297.029702970297,
297.029702970297: d4~297.029702970297`
let playback = playTune(music, Infinity)
for (let i = 0; i < rows; i ++){
  const row = [];
  for (let j = 0; j < cols; j ++){ row.push(emptyColor); }
  board.push(row);
}

const pieces = [ iPiece, jPiece, lPiece, oPiece,
                        sPiece, tPiece, zPiece ]
const colors = [ "3", "6", "8",
                             "1", "7", "4", "5" ]

let fallingPiece;
let fallingPieceColor;
let numFallingPieceRows, numFallingPieceCols;
let fallingPieceRow, fallingPieceCol;

function newFallingPiece() {
  let randomIndex = Math.floor(Math.random() * pieces.length);
  fallingPiece = pieces[randomIndex]
  fallingPieceColor = colors[randomIndex]

  numFallingPieceRows = fallingPiece.length;
  numFallingPieceCols = fallingPiece[0].length;
  fallingPieceRow = 0;
  fallingPieceCol = Math.floor(cols / 2) - Math.floor(numFallingPieceCols / 2)
}

function placeFallingPiece() {
  let fp = fallingPiece;
  for (let r = 0; r < fp.length; r ++) {
    const row = fp[r]
    for (let c = 0; c < fp[r].length; c++) {
      const col = fp[r][c];
      if (col) {
        let boardRow = fallingPieceRow + r;
        let boardCol = fallingPieceCol + c;
        board[boardRow][boardCol] = fallingPieceColor
      }
    }
  }
  removeFullRows()
}

function generateEmpty2DList(rows, cols, fill) {
  const grid = [];
  for (let i = 0; i < rows; i ++) {
    const row = [];
    for (let j = 0; j < cols; j ++) {
      row.push(fill ? fill : "");
    }
    grid.push(row);
  }
  return grid
}

function rotateFallingPiece() {
  const oldRows = numFallingPieceRows;
  const oldCols = numFallingPieceCols;
  const oldPiece = fallingPiece;
  const oldRow = fallingPieceRow;
  const oldCol = fallingPieceCol;
  let rotated = generateEmpty2DList(oldCols, oldRows)

  let newRows = oldCols
  let newCols = oldRows

  for (let c = 0; c < oldCols; c ++) {
    for (let r = 0; r < oldRows; r ++) {
      rotated[c][r] = oldPiece[r][c];
    }
  }

  for (let r = 0; r < newRows; r ++) {
    rotated[r].reverse()
  }

  numFallingPieceRows = newRows;
  numFallingPieceCols = newCols;
  fallingPiece = rotated;

  let newRow = oldRow + Math.floor(oldRows / 2) - Math.floor(newRows / 2)
  let newCol = oldCol + Math.floor(oldCols / 2) - Math.floor(newCols / 2)

  fallingPieceRow = newRow
  fallingPieceCol = newCol;

  if(!fallingPieceIsLegal()) {
    fallingPiece = oldPiece;
    numFallingPieceRows = oldRows;
    numFallingPieceCols = oldCols;
    fallingPieceRow = oldRow;
    fallingPieceCol = oldCol;
  }
  
}

function fallingPieceIsLegal() {
  for (let r = 0; r < fallingPiece.length; r ++) {
    const row = fallingPiece[r]
    for (let c = 0; c < row.length; c ++) {
      const col = fallingPiece[r][c];
      if (!col) continue;
      const x = r + fallingPieceRow;
      const y = c + fallingPieceCol;
      const withinBoundsX = x >= 0 && x < rows;
      const withinBoundsY = y >= 0 && y < cols;
      if (!withinBoundsX || !withinBoundsY) {
        return false;
      }
      if (board[x][y] != emptyColor) {
        return false
      }
    }
  }
  return true;
}

function moveFallingPiece(drow, dcol) {
  fallingPieceRow += drow;
  fallingPieceCol += dcol;
  if (!fallingPieceIsLegal()) {
    fallingPieceRow -= drow;
    fallingPieceCol -= dcol;
    return false;
  }
  return true;
}

function removeFullRows(app) {
  let fullRows = 0;
  const newBoard = [];
  board.forEach(row => {
    let isFull = true;
    row.forEach(col => {
      if (col == emptyColor) {
        isFull = false;
      }
    })
    if (isFull) {
      fullRows += 1;
    }
    else {
      newBoard.push(row)
    }
  })
  for (let r = 0; r < fullRows; r ++) {
    newBoard.splice(0, 0, Array.from(emptyColor.repeat(cols)))
  }
  board = newBoard
  score += fullRows;
}

// each "sprite" contains 16 actual cells, x, y are the top left coords
function genPiece(xCoord, yCoord) {
  let sprite = []; // 4x4 of 4x4s
  for (let i = 0; i < 4; i ++) {
    let rows = []; // 4 4x4s
    for (let j = 0; j < 4; j ++){
      const x = i + xCoord;
      const y = j + yCoord;
      let cell = board[x][y];
      const withinFallingPieceX = fallingPieceRow <= x && x < fallingPieceRow + numFallingPieceRows
      const withinFallingPieceY = fallingPieceCol <= y && y < fallingPieceCol + numFallingPieceCols
      if (withinFallingPieceX && withinFallingPieceY) {
        if (fallingPiece[x - fallingPieceRow][y - fallingPieceCol]) {
          cell = fallingPieceColor;
        }
      }
      let row = []; // 4x4
      for (let r = 0; r < 4; r ++) {
        let miniRow = []; // 1x4
        for (let c = 0; c < 4; c ++) {
          if ((r == 0 || c == 0 || r == 3 || c == 3) && borders) { // make borders black
            miniRow.push(emptyColor);
          }
          else {
            miniRow.push(cell);
          }
        }
        row.push(miniRow);
      }
      rows.push(row)
    }
    sprite.push(rows);
  }
  return boardToString(sprite);
}

function boardToString(board) {
  // board is a 4x4 of 4x4s
  let string = "";
  for (const bigRow of board) {
    let bigRowString = "\n\n\n";
    for (const cell of bigRow) {
      // 4x4
      let row = "";
      for (let c = 0; c < 4; c ++) {
        for (let r = 0; r < 4; r ++) {
          row += (cell[r][c]);
        }
        row += "\n"
      }

      let rows = bigRowString.split("\n")
      let newRows = row.trim().split("\n")
      bigRowString = rows.map((r, i) => r + newRows[i]).join("\n")
    }
    string += bigRowString
    string += "\n"
  }
  return string;
}


function loadPieces() {
  const legend = [];
  let i = 0;
  for (let r = 0; r < rows; r += 4){
    for (let c = 0; c < cols; c += 4) {
      legend.push([`${i}`, genPiece(r, c)])
      i += 1;
    }
  }
  setLegend(...legend)
}

function start() {
  fallingPiece = undefined;
  fallingPieceRow = undefined;
  fallingPieceCol = undefined;
  fallingPieceColor = undefined;
  numFallingPieceRows = undefined;
  numFallingPieceCols = undefined;

  board = generateEmpty2DList(rows, cols, emptyColor)
  newFallingPiece();
  loadPieces();
  score = 0;
}


start();
setBackground(bitmap`
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
0000000000000000`);

setMap(`
01
23
45
`)

setSolids([]);

setInterval(() => {
  getAll().forEach((sprite) => {
    sprite.remove();
  })
  loadPieces();
  setMap(`
  01
  23
  45
  `)

  clearText();
  addText(`${score}`, { x: 14, y: 1, color: [255, 255, 255] })
}, 30)

setInterval(() => {
  if (!moveFallingPiece(1, 0)) {
    placeFallingPiece();
    newFallingPiece();
  }

  for (let c = 0; c < cols; c ++) {
    if (board[0][c] != emptyColor) {
      start();
    }
  }
}, 700)

onInput("d", () => { moveFallingPiece(0, 1) })
onInput("a", () => { moveFallingPiece(0, -1) })
onInput("s", () => { moveFallingPiece(1, 0) })
onInput("w", () => { rotateFallingPiece() })
onInput("k", () => { start() })

