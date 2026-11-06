const test = require('node:test');
const assert = require('node:assert/strict');
const SnakeLogic = require('./snake-logic.js');

function createState(overrides) {
    return SnakeLogic.createInitialState(
        {
            gridSize: 8,
            snake: [
                { x: 3, y: 3 },
                { x: 2, y: 3 },
                { x: 1, y: 3 }
            ],
            direction: 'right',
            food: { x: 6, y: 6 },
            score: 0,
            status: 'running',
            tick: 0,
            pendingDirection: null,
            ...(overrides || {})
        },
        () => 0
    );
}

test('moves one cell each tick', () => {
    const state = createState();
    const next = SnakeLogic.stepGame(state, () => 0);

    assert.deepEqual(next.snake, [
        { x: 4, y: 3 },
        { x: 3, y: 3 },
        { x: 2, y: 3 }
    ]);
    assert.equal(next.score, 0);
    assert.equal(next.tick, 1);
    assert.equal(next.status, 'running');
});

test('ignores immediate reverse direction', () => {
    const state = createState();
    const queued = SnakeLogic.queueDirection(state, 'left');

    assert.equal(queued.pendingDirection, null);
});

test('grows and increments score when food is eaten', () => {
    const state = createState({ food: { x: 4, y: 3 } });
    const next = SnakeLogic.stepGame(state, () => 0);

    assert.equal(next.score, 1);
    assert.equal(next.snake.length, 4);
    assert.equal(next.status, 'running');
    assert.ok(next.food);
    assert.ok(!next.snake.some((segment) => SnakeLogic.positionsEqual(segment, next.food)));
});

test('detects wall collision', () => {
    const state = createState({
        snake: [
            { x: 7, y: 2 },
            { x: 6, y: 2 },
            { x: 5, y: 2 }
        ],
        direction: 'right'
    });

    const next = SnakeLogic.stepGame(state, () => 0);
    assert.equal(next.status, 'gameOver');
});

test('detects self collision', () => {
    const state = createState({
        snake: [
            { x: 3, y: 2 },
            { x: 3, y: 3 },
            { x: 2, y: 3 },
            { x: 2, y: 2 }
        ],
        direction: 'down',
        food: { x: 0, y: 0 }
    });

    const next = SnakeLogic.stepGame(state, () => 0);
    assert.equal(next.status, 'gameOver');
});

test('allows moving into the current tail cell when not growing', () => {
    const state = createState({
        snake: [
            { x: 2, y: 2 },
            { x: 2, y: 3 },
            { x: 1, y: 3 },
            { x: 1, y: 2 }
        ],
        direction: 'left',
        food: { x: 0, y: 0 }
    });

    const next = SnakeLogic.stepGame(state, () => 0);
    assert.equal(next.status, 'running');
    assert.deepEqual(next.snake[0], { x: 1, y: 2 });
});

test('places food only in free cells', () => {
    const snake = [];
    for (let y = 0; y < 4; y += 1) {
        for (let x = 0; x < 4; x += 1) {
            if (!(x === 3 && y === 3)) {
                snake.push({ x, y });
            }
        }
    }

    const food = SnakeLogic.createFood(4, snake, () => 0.42);
    assert.deepEqual(food, { x: 3, y: 3 });
});
