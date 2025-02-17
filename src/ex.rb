require 'curses'

include Curses

init_screen
start_color

init_pair(1, 1, 0)
init_pair(2, 2, 0)
init_pair(3, 3, 0)
init_pair(4, 4, 0)
init_pair(5, 5, 0)
init_pair(6, 6, 0)
init_pair(7, 7, 0)
curs_set(0)
noecho

$log = File.open('log.txt', 'w')

$score = 0

BOARD = Array.new(22) { Array.new(10, ' ') }
def add_to_board(block, x, y)
  rows = block.rows
  rows.each_with_index { |row, idx|
    row.split('').each_with_index { |cell, i|
      BOARD[y + idx + block.y][x + i + block.x] = if cell == ' '
        BOARD[y + idx + block.y][x + i + block.x]
      else
        cell
      end
    }
  }
end

def can_drop?(block, x, y)
  return can_move_to?(block, x, y+1)
end

def can_move_to?(block, x, y)
  return false if x < 0 - block.x or x + block.x + block.width > 10
  return false if y + block.y + block.height > HEIGHT
  block_rows = block.rows.map { |r| r.split('') }

  block_rows.each_with_index { |block_row, idx|
    board_row = BOARD[y+block.y+idx].slice(x+block.x, block_row.length)
    board_row.zip(block_row).each { |zipped_elem|
      return false if zipped_elem[0] != ' ' && zipped_elem[1] != ' '
    }
  }

  return true
end

def square(cell)
  if cell == ' '
    '  '
  else
    '[]'
  end
end

def rotate_r(block, x, y)
  # $log.puts("block is: #{block.inspect}")
  block.spin_forward
  block.spin_backwards unless can_move_to?(block, x, y)

  return block, x, y
end

def delete_full_rows
  to_delete = []
  BOARD.each_with_index { |row, idx|
    if row.all? { |cell| cell != ' ' }
      to_delete.push idx
    end
  }

  $score += to_delete.size * to_delete.size

  to_delete.each { |idx|
    BOARD.delete_at(idx)
    BOARD.unshift(Array.new(10, ' '))
  }
end

class Block
  MAPPING = {
    I: [["6666", 0, 1], ["6\n6\n6\n6", 2, 0], ["6666", 0, 2], ["6\n6\n6\n6", 1, 0]],
    J: [["4\n444", 1, 1], ["44\n4\n4", 2, 0], ["444\n  4", 1, 1], [" 4\n 4\n44", 2, 0]],
    L: [["777\n7", 1, 1], ["77\n 7\n 7", 2, 0], ["  7\n777", 1, 1], ["7\n7\n77", 2, 0]],
    O: [["33\n33", 1, 1]],
    T: [["555\n 5", 1, 1], ["5\n55\n5", 2, 0], [" 5 \n555", 1, 1], [" 5\n55\n 5", 2, 0]],
    S: [[" 22\n22", 1, 1], ["2\n22\n 2", 2, 0]],
    Z: [["11\n 11", 1, 1], [" 1\n11\n1", 2, 0]]
  }

  def initialize(shape, rotation = 0)
    @shape = shape
    @rotation = rotation
  end

  def x
    MAPPING[@shape][@rotation][1]
  end

  def y
    MAPPING[@shape][@rotation][2]
  end

  def spin_forward
    @rotation = (@rotation + 1) % MAPPING[@shape].size
  end

  def spin_backwards
    @rotation = (@rotation - 1) % MAPPING[@shape].size
  end

  def rows
    spin_to_current[0].split("\n")
  end

  def spin_to_current
    positions = MAPPING[@shape]
    positions[@rotation % positions.size]
  end

  def width
    rows.map(&:length).max
  end

  def height
    rows.size
  end
end

def render_squares(win, row)
  row.split('').each { |ch|
    win.attron(color_pair(ch.to_i)) { win << square(ch) }
  }
end

def render_next_block(win, block)
  x = 15
  y = 2

  (0..3).each { |i|
    win.setpos(y + i, x * 2)
    win << "                "
  }

  block.rows.each_with_index { |row, i|
    win.setpos(y + i + block.y, (x + 1 + block.x) * 2)
    render_squares(win, row)
  }
  win.refresh
end

HEIGHT = 22

if __FILE__ == $0
  begin
    win = Curses::Window.new(0, 0, 1, 2)
    x = 4
    y = 0
    wait = 0.8
    block_count = 0
    blocks_per_level = 15
    start_cycle = Process.clock_gettime(Process::CLOCK_MONOTONIC)
    curr_block = Block.new([:I, :J, :L, :O, :S, :Z,:T].sample, 0)
    next_block = Block.new([:I, :J, :L, :O, :S, :Z,:T].sample, 0)
    render_next_block(win, next_block)
    loop do
      cb_rows = curr_block.rows
      (0..HEIGHT-1).each { |h|
        win.setpos(h, 0)
        win << "##"
        render_squares(win, BOARD[h].join)
        win.setpos(h, 11*2)
        win << "##"
        cb_rows.each_with_index { |cbr, i|
          offset = (cbr.size - cbr.lstrip.size)
          win.setpos(y+i + curr_block.y, (x + 1 + curr_block.x + offset) * 2)
          render_squares(win, cbr.strip)
        }
        win.refresh
      }
      win.setpos(HEIGHT, 0)
        win << ("##" * 12)

      win.setpos(HEIGHT+3, 4)
        win << ("Score #{$score * 100}")

      str = STDIN.read_nonblock(1, exception: false)
      case str
      when 'h'
        x = x-1 if can_move_to?(curr_block, x-1, y)
      when 'l'
        x = x+1 if can_move_to?(curr_block, x+1, y)
      when 'j'
        y = y+1 if can_move_to?(curr_block, x, y+1)
      when ' '
        curr_block, x, y = rotate_r(curr_block, x, y)
      end


      now = Process.clock_gettime(Process::CLOCK_MONOTONIC)
      elapsed = now - start_cycle
      if elapsed > wait
        if can_drop?(curr_block, x, y)
          y += 1
        else
          # $log.puts("curr_block, x, y: #{curr_block}, #{x}, #{y}")
          add_to_board(curr_block, x, y)
          block_count += 1
          wait = [wait - 0.1, 0.05].max if block_count % blocks_per_level == 0
          delete_full_rows
          y = 0
          x = 4
          curr_block = next_block
          next_block = Block.new([:I, :J, :L, :O, :S, :Z,:T].sample, 0)
          render_next_block(win, next_block)
          unless can_move_to?(curr_block, x, y)
            win.setpos(8, 0)
            win << "                        "
            win.setpos(9, 0)
            win << "       GAME OVER        "
            win.setpos(10, 0)
            win << "                        "
            win.setpos(11, 0)
            win << "   PRESS ENTER TO EXIT  "
            win.setpos(12, 0)
            win << "                        "
            loop {
              c = win.getch
              break if c == 10 || c == 13
            }
            break
          end
        end
        start_cycle = now
      end
    end
  ensure
    close_screen
    $log.close
  end
end
