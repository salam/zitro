(function () {
    'use strict';

    const logic = window.SnakeLogic;
    if (!logic) {
        return;
    }

    const boardElement = document.getElementById('snakeBoard');
    const scoreElement = document.getElementById('snakeScore');
    const statusElement = document.getElementById('snakeStatus');
    const pauseButton = document.getElementById('snakePauseBtn');
    const restartButton = document.getElementById('snakeRestartBtn');
    const controlButtons = document.querySelectorAll('[data-direction]');

    if (!boardElement || !scoreElement || !statusElement || !pauseButton || !restartButton) {
        return;
    }

    const GRID_SIZE = 16;
    const TICK_MS = 140;

    let state = logic.createInitialState({ gridSize: GRID_SIZE });
    const cells = [];

    function boardIndex(position) {
        return position.y * state.gridSize + position.x;
    }

    function buildBoard() {
        boardElement.style.setProperty('--snake-grid-size', String(state.gridSize));

        const fragment = document.createDocumentFragment();
        const totalCells = state.gridSize * state.gridSize;
        for (let index = 0; index < totalCells; index += 1) {
            const cell = document.createElement('div');
            cell.className = 'snake-cell';
            cells.push(cell);
            fragment.appendChild(cell);
        }

        boardElement.appendChild(fragment);
    }

    function clearBoardState() {
        for (const cell of cells) {
            cell.classList.remove('snake-cell-head', 'snake-cell-body', 'snake-cell-food');
        }
    }

    function applySnakeToBoard() {
        state.snake.forEach((segment, index) => {
            const cell = cells[boardIndex(segment)];
            if (!cell) {
                return;
            }
            cell.classList.add(index === 0 ? 'snake-cell-head' : 'snake-cell-body');
        });
    }

    function applyFoodToBoard() {
        if (!state.food) {
            return;
        }
        const cell = cells[boardIndex(state.food)];
        if (cell) {
            cell.classList.add('snake-cell-food');
        }
    }

    function getStatusLabel(status) {
        if (status === 'running') {
            return 'Running';
        }
        if (status === 'paused') {
            return 'Paused';
        }
        if (status === 'gameOver') {
            return 'Game over';
        }
        return status;
    }

    function render() {
        clearBoardState();
        applySnakeToBoard();
        applyFoodToBoard();

        scoreElement.textContent = String(state.score);
        statusElement.textContent = getStatusLabel(state.status);
        pauseButton.textContent = state.status === 'paused' ? 'Resume' : 'Pause';
        pauseButton.disabled = state.status === 'gameOver';
    }

    function tick() {
        state = logic.stepGame(state);
        render();
    }

    function setDirection(direction) {
        state = logic.queueDirection(state, direction);
        render();
    }

    function togglePause() {
        state = logic.togglePause(state);
        render();
    }

    function restartGame() {
        state = logic.restartGame(state);
        render();
    }

    function handleKeydown(event) {
        const key = event.key.toLowerCase();
        const directionByKey = {
            arrowup: 'up',
            w: 'up',
            arrowdown: 'down',
            s: 'down',
            arrowleft: 'left',
            a: 'left',
            arrowright: 'right',
            d: 'right'
        };

        const direction = directionByKey[key];
        if (direction) {
            event.preventDefault();
            setDirection(direction);
            return;
        }

        if (key === ' ' || key === 'p') {
            event.preventDefault();
            togglePause();
            return;
        }

        if (key === 'r') {
            event.preventDefault();
            restartGame();
            return;
        }

        if (key === 'enter' && state.status === 'gameOver') {
            event.preventDefault();
            restartGame();
        }
    }

    function wireControls() {
        document.addEventListener('keydown', handleKeydown);

        controlButtons.forEach((button) => {
            button.addEventListener('click', () => {
                const direction = button.getAttribute('data-direction');
                if (direction) {
                    setDirection(direction);
                }
            });
        });

        pauseButton.addEventListener('click', togglePause);
        restartButton.addEventListener('click', restartGame);
    }

    buildBoard();
    wireControls();
    render();
    window.setInterval(tick, TICK_MS);
})();
