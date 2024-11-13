use crossterm::{
    cursor,
    event::{self, KeyCode, KeyEvent},
    execute, queue,
    style::{Color, Print, SetBackgroundColor},
    terminal::{self, Clear, ClearType},
};
use rand::Rng;
use std::{io::{self, Write}, thread, time::Duration};

const WIDTH: usize = 10;
const HEIGHT: usize = 20;

#[derive(Clone, Copy, PartialEq)]
enum Block {
    Empty,
    Filled(Color),
}

#[derive(Clone, Copy)]
enum Shape {
    I, O, T, S, Z, L, J,
}

struct Tetromino {
    shape: Shape,
    x: isize,
    y: isize,
    rotation: u8,
}

impl Tetromino {
    fn new(shape: Shape) -> Self {
        Self { shape, x: (WIDTH / 2 - 1) as isize, y: 0, rotation: 0 }
    }

    fn blocks(&self) -> Vec<(isize, isize)> {
        // Define block patterns for each shape and rotation
        match self.shape {
            Shape::I => match self.rotation % 2 {
                0 => vec![(0, 1), (1, 1), (2, 1), (3, 1)],
                _ => vec![(2, 0), (2, 1), (2, 2), (2, 3)],
            },
            Shape::O => vec![(1, 0), (2, 0), (1, 1), (2, 1)],
            Shape::T => match self.rotation % 4 {
                0 => vec![(1, 0), (0, 1), (1, 1), (2, 1)],
                1 => vec![(1, 0), (1, 1), (1, 2), (2, 1)],
                2 => vec![(0, 1), (1, 1), (2, 1), (1, 2)],
                _ => vec![(1, 0), (1, 1), (1, 2), (0, 1)],
            },
            Shape::S => match self.rotation % 2 {
                0 => vec![(1, 0), (2, 0), (0, 1), (1, 1)],
                _ => vec![(1, 0), (1, 1), (2, 1), (2, 2)],
            },
            Shape::Z => match self.rotation % 2 {
                0 => vec![(0, 0), (1, 0), (1, 1), (2, 1)],
                _ => vec![(2, 0), (1, 1), (2, 1), (1, 2)],
            },
            Shape::L => match self.rotation % 4 {
                0 => vec![(0, 0), (0, 1), (1, 1), (2, 1)],
                1 => vec![(1, 0), (1, 1), (1, 2), (2, 2)],
                2 => vec![(0, 1), (1, 1), (2, 1), (2, 2)],
                _ => vec![(1, 0), (1, 1), (1, 2), (0, 0)],
            },
            Shape::J => match self.rotation % 4 {
                0 => vec![(2, 0), (0, 1), (1, 1), (2, 1)],
                1 => vec![(1, 0), (1, 1), (1, 2), (2, 0)],
                2 => vec![(0, 1), (1, 1), (2, 1), (0, 2)],
                _ => vec![(1, 0), (1, 1), (1, 2), (0, 2)],
            },
        }
    }
}

struct Game {
    board: Vec<Vec<Block>>,
    current_piece: Tetromino,
    score: u32,
    running: bool,
}

impl Game {
    fn new() -> Self {
        let piece = Self::random_piece();
        Self {
            board: vec![vec![Block::Empty; WIDTH]; HEIGHT],
            current_piece: piece,
            score: 0,
            running: true,
        }
    }

    fn random_piece() -> Tetromino {
        let shape = match rand::thread_rng().gen_range(0..7) {
            0 => Shape::I,
            1 => Shape::O,
            2 => Shape::T,
            3 => Shape::S,
            4 => Shape::Z,
            5 => Shape::L,
            _ => Shape::J,
        };
        Tetromino::new(shape)
    }

    fn draw(&self) {
        let mut stdout = io::stdout();
        queue!(stdout, cursor::MoveTo(0, 0), Clear(ClearType::All)).unwrap();
    
        // Draw top border
        queue!(stdout, Print("╔")).unwrap();
        for _ in 0..WIDTH {
            queue!(stdout, Print("══")).unwrap();
        }
        queue!(stdout, Print("╗\n")).unwrap();
    
        // Draw board with left and right borders
        for row in self.board.iter() {
            queue!(stdout, Print("║")).unwrap(); // Left border
            for &block in row.iter() {
                match block {
                    Block::Empty => queue!(stdout, Print("  ")).unwrap(),
                    Block::Filled(color) => {
                        queue!(
                            stdout,
                            SetBackgroundColor(color),
                            Print("  "),
                            SetBackgroundColor(Color::Reset)
                        )
                        .unwrap();
                    }
                }
            }
            queue!(stdout, Print("║\n")).unwrap(); // Right border
        }
    
        // Draw bottom border
        queue!(stdout, Print("╚")).unwrap();
        for _ in 0..WIDTH {
            queue!(stdout, Print("══")).unwrap();
        }
        queue!(stdout, Print("╝\n")).unwrap();
    
        // Draw current piece without adjusting `x` for the left border or `y` for the top border
        for (dx, dy) in self.current_piece.blocks() {
            let x = (self.current_piece.x + dx) as usize;
            let y = (self.current_piece.y + dy) as usize;
            if y < HEIGHT && x < WIDTH {
                queue!(
                    stdout,
                    cursor::MoveTo((x * 2 + 1) as u16, (y + 1) as u16), // Adjust x for alignment within the border
                    SetBackgroundColor(Color::Blue),
                    Print("  "),
                    SetBackgroundColor(Color::Reset)
                )
                .unwrap();
            }
        }
    
        stdout.flush().unwrap();
    }
    

    fn game_loop(&mut self) {
        let mut last_update = std::time::Instant::now();
        while self.running {
            if std::time::Instant::now().duration_since(last_update) > Duration::from_millis(500) {
                self.move_piece_down();
                last_update = std::time::Instant::now();
            }

            if event::poll(Duration::from_millis(10)).unwrap() {
                if let event::Event::Key(KeyEvent { code, .. }) = event::read().unwrap() {
                    match code {
                        KeyCode::Left => self.move_piece(-1),
                        KeyCode::Right => self.move_piece(1),
                        KeyCode::Down => self.move_piece_down(),
                        KeyCode::Up => self.rotate_piece(),
                        KeyCode::Char('q') => self.running = false,
                        _ => {}
                    }
                }
            }
            self.draw();
            thread::sleep(Duration::from_millis(50));
        }
    }

    fn move_piece(&mut self, dx: isize) {
        self.current_piece.x += dx;
        if !self.valid_position() {
            self.current_piece.x -= dx;
        }
    }

    fn move_piece_down(&mut self) {
        self.current_piece.y += 1;
        if !self.valid_position() {
            self.current_piece.y -= 1;
            self.lock_piece();
            self.clear_lines();
            self.current_piece = Self::random_piece();
            if !self.valid_position() {
                self.running = false;
            }
        }
    }

    fn rotate_piece(&mut self) {
        self.current_piece.rotation += 1;
        if !self.valid_position() {
            self.current_piece.rotation -= 1;
        }
    }

    fn valid_position(&self) -> bool {
        for (dx, dy) in self.current_piece.blocks() {
            let x = self.current_piece.x + dx;
            let y = self.current_piece.y + dy;
            if x < 0 || x >= WIDTH as isize || y >= HEIGHT as isize || self.board[y as usize][x as usize] != Block::Empty {
                return false;
            }
        }
        true
    }

    fn lock_piece(&mut self) {
        for (dx, dy) in self.current_piece.blocks() {
            let x = (self.current_piece.x + dx) as usize;
            let y = (self.current_piece.y + dy) as usize;
            if y < HEIGHT && x < WIDTH {
                self.board[y][x] = Block::Filled(Color::Blue);
            }
        }
    }

    fn clear_lines(&mut self) {
        self.board.retain(|row| row.iter().any(|&block| block == Block::Empty));
        while self.board.len() < HEIGHT {
            self.board.insert(0, vec![Block::Empty; WIDTH]);
            self.score += 100;
        }
    }
}

fn main() {
    let mut game = Game::new();
    execute!(io::stdout(), terminal::EnterAlternateScreen).unwrap();
    terminal::enable_raw_mode().unwrap();
    game.game_loop();
    terminal::disable_raw_mode().unwrap();
    execute!(io::stdout(), terminal::LeaveAlternateScreen).unwrap();
}
