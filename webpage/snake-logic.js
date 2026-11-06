(function () {
    'use strict';

    const DIRECTIONS = Object.freeze({
        up: { x: 0, y: -1 },
        down: { x: 0, y: 1 },
        left: { x: -1, y: 0 },
        right: { x: 1, y: 0 }
    });

    const VALID_STATUSES = new Set(['running', 'paused', 'gameOver']);

    function clonePosition(position) {
        return { x: position.x, y: position.y };
    }

    function cloneSnake(snake) {
        return snake.map(clonePosition);
    }

    function normalizeDirection(direction) {
        return Object.prototype.hasOwnProperty.call(DIRECTIONS, direction) ? direction : null;
    }

    function normalizeStatus(status) {
        return VALID_STATUSES.has(status) ? status : 'running';
    }

    function resolveGridSize(gridSize) {
        return Number.isInteger(gridSize) && gridSize >= 5 ? gridSize : 16;
    }

    function toSafeRandomValue(randomFn) {
        const rawValue = typeof randomFn === 'function' ? randomFn() : Math.random();
        if (!Number.isFinite(rawValue)) {
            return 0;
        }
        if (rawValue <= 0) {
            return 0;
        }
        if (rawValue >= 1) {
            return 0.999999999;
        }
        return rawValue;
    }

    function positionsEqual(a, b) {
        return a.x === b.x && a.y === b.y;
    }

    function isOutOfBounds(position, gridSize) {
        return (
            position.x < 0 ||
            position.y < 0 ||
            position.x >= gridSize ||
            position.y >= gridSize
        );
    }

    function isOppositeDirection(currentDirection, nextDirection) {
        const current = DIRECTIONS[currentDirection];
        const next = DIRECTIONS[nextDirection];
        if (!current || !next) {
            return false;
        }
        return current.x + next.x === 0 && current.y + next.y === 0;
    }

    function createInitialSnake(gridSize) {
        const center = Math.floor(gridSize / 2);
        return [
            { x: Math.min(center + 1, gridSize - 1), y: center },
            { x: center, y: center },
            { x: Math.max(center - 1, 0), y: center }
        ];
    }

    function createFood(gridSize, snake, randomFn) {
        const occupied = new Set(snake.map((segment) => `${segment.x},${segment.y}`));
        const availableCells = [];

        for (let y = 0; y < gridSize; y += 1) {
            for (let x = 0; x < gridSize; x += 1) {
                const key = `${x},${y}`;
                if (!occupied.has(key)) {
                    availableCells.push({ x, y });
                }
            }
        }

        if (availableCells.length === 0) {
            return null;
        }

        const index = Math.floor(toSafeRandomValue(randomFn) * availableCells.length);
        return availableCells[index];
    }

    function createInitialState(options, randomFn) {
        const safeOptions = options || {};
        const gridSize = resolveGridSize(safeOptions.gridSize);
        const snake =
            Array.isArray(safeOptions.snake) && safeOptions.snake.length > 0
                ? cloneSnake(safeOptions.snake)
                : createInitialSnake(gridSize);

        const direction = normalizeDirection(safeOptions.direction) || 'right';
        const pendingDirection = normalizeDirection(safeOptions.pendingDirection);
        const food =
            safeOptions.food && Number.isInteger(safeOptions.food.x) && Number.isInteger(safeOptions.food.y)
                ? clonePosition(safeOptions.food)
                : createFood(gridSize, snake, randomFn);

        return {
            gridSize,
            snake,
            direction,
            pendingDirection: pendingDirection || null,
            food,
            score: Number.isInteger(safeOptions.score) ? safeOptions.score : 0,
            status: normalizeStatus(safeOptions.status),
            tick: Number.isInteger(safeOptions.tick) ? safeOptions.tick : 0
        };
    }

    function queueDirection(state, nextDirection) {
        if (state.status === 'gameOver') {
            return state;
        }

        const normalizedDirection = normalizeDirection(nextDirection);
        if (!normalizedDirection) {
            return state;
        }

        const activeDirection = state.pendingDirection || state.direction;
        if (state.snake.length > 1 && isOppositeDirection(activeDirection, normalizedDirection)) {
            return state;
        }

        return {
            ...state,
            pendingDirection: normalizedDirection
        };
    }

    function togglePause(state) {
        if (state.status === 'gameOver') {
            return state;
        }

        if (state.status === 'paused') {
            return {
                ...state,
                status: 'running'
            };
        }

        if (state.status === 'running') {
            return {
                ...state,
                status: 'paused'
            };
        }

        return state;
    }

    function stepGame(state, randomFn) {
        if (state.status !== 'running') {
            return state;
        }

        const activeDirection = state.pendingDirection || state.direction;
        const currentHead = state.snake[0];
        const vector = DIRECTIONS[activeDirection];
        const nextHead = {
            x: currentHead.x + vector.x,
            y: currentHead.y + vector.y
        };

        if (isOutOfBounds(nextHead, state.gridSize)) {
            return {
                ...state,
                direction: activeDirection,
                pendingDirection: null,
                status: 'gameOver'
            };
        }

        const willGrow = Boolean(state.food) && positionsEqual(nextHead, state.food);
        const collisionBody = willGrow ? state.snake : state.snake.slice(0, -1);
        const hitsBody = collisionBody.some((segment) => positionsEqual(segment, nextHead));

        if (hitsBody) {
            return {
                ...state,
                direction: activeDirection,
                pendingDirection: null,
                status: 'gameOver'
            };
        }

        const nextSnake = [nextHead].concat(cloneSnake(state.snake));
        if (!willGrow) {
            nextSnake.pop();
        }

        let nextFood = state.food ? clonePosition(state.food) : null;
        let nextScore = state.score;
        let nextStatus = 'running';

        if (willGrow) {
            nextScore += 1;
            nextFood = createFood(state.gridSize, nextSnake, randomFn);
            if (!nextFood) {
                nextStatus = 'gameOver';
            }
        }

        return {
            ...state,
            snake: nextSnake,
            direction: activeDirection,
            pendingDirection: null,
            food: nextFood,
            score: nextScore,
            status: nextStatus,
            tick: state.tick + 1
        };
    }

    function restartGame(state, randomFn) {
        return createInitialState(
            {
                gridSize: state.gridSize,
                status: 'running'
            },
            randomFn
        );
    }

    const SnakeLogic = Object.freeze({
        DIRECTIONS,
        createFood,
        createInitialState,
        queueDirection,
        restartGame,
        stepGame,
        togglePause,
        positionsEqual,
        isOutOfBounds
    });

    if (typeof module !== 'undefined' && module.exports) {
        module.exports = SnakeLogic;
    }

    if (typeof window !== 'undefined') {
        window.SnakeLogic = SnakeLogic;
    }
})();
