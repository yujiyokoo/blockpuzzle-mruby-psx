BOARD = Array.new(20) { Array.new(10, ' ') }
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

def square(cell)
  if cell == ' '
    ' '
  else
    '['
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
  row.each { |ch|
    # TODO: colours
    win << square(ch)
  }
end

def render_next_block(win, block)
  x = 45
  y = 2

  block.rows.each_with_index { |row, i|
    # PsxMruby.print_msg("setting x, y to: #{y + i + block.y} #{x + 1 + block.x}")
    win.setpos(y + i + block.y, (x + 1 + block.x))
    render_squares(win, row.split(''))
  }
end

class FakeCurses
  def initialize
    @x = @y = 0
  end

  def setpos(y, x)
    @x = x
    @y = y
  end

  def <<(str)
    # PsxMruby.print_msg("incoming str: #{str.inspect}")
    chars = str.split('')
    chars.each { |ch|
      case
      when ch == '#'
        PsxMruby.draw_rect(110 + @x * 10, 20 + @y * 10, 10, 10, 255, 255, 255)
      when ch == '['
        PsxMruby.draw_rect(110 + @x * 10, 20 + @y * 10, 10, 10, 255, 0, 0)
      end
      @x += 1
    }
  end
end

$score = 0

class MainGame
  HEIGHT = 20

  def initialize
    # TODO: randomness
    @curr_random = 0
  end

  def random_block
    # TODO: randomness
    blocks = [:I, :J, :L, :O, :S, :Z,:T]
    #blocks[rand(blocks.size)]
    @curr_random = (@curr_random += 1) % blocks.size
    blocks[@curr_random]
  end

  def draw_walls_and_bg(win)
    PsxMruby.draw_rect(110, 20, 10, 200, 255, 255, 255)
    PsxMruby.draw_rect(220, 20, 10, 200, 255, 255, 255)
    PsxMruby.draw_rect(110, 10, 120, 10, 255, 255, 255)
    PsxMruby.draw_rect(110, 220, 120, 10, 255, 255, 255)
    PsxMruby.draw_rect(120, 20, 100, 200, 0, 0, 0)
  end

  def main_loop
    PsxMruby.init_context
    win = FakeCurses.new
    x = 4
    y = 0
    current_frame = 0
    wait_frames = 3
    block_count = 0
    blocks_per_level = 15
    start_frame = 0
    curr_block = Block.new(random_block, 0)
    next_block = Block.new(random_block, 0)
    render_next_block(win, next_block)
    loop do
      cb_rows = curr_block.rows
      draw_walls_and_bg(win)

      PsxMruby.flip_buffers
      current_frame += 1

      BOARD.each_with_index { |row, i|
        win.setpos(i, 0)
        render_squares(win, row)
      }

      cb_rows.each_with_index { |cbr, i|
        offset = (cbr.size - cbr.strip.size)
        win.setpos(y+i + curr_block.y, (x + curr_block.x + offset))
        render_squares(win, cbr.strip.split(''))
      }

      win.setpos(HEIGHT+3, 4)
      # win << ("Score #{$score * 100}")

      # TODO:input
      # str = STDIN.read_nonblock(1, exception: false)
      str = 'a'
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

      elapsed = current_frame - start_frame
      # PsxMruby.print_msg("------ elapsed: #{elapsed}")
      if elapsed > wait_frames
        if can_drop?(curr_block, x, y)
          y += 1
        else
          # $log.puts("curr_block, x, y: #{curr_block}, #{x}, #{y}")
          add_to_board(curr_block, x, y)
          block_count += 100
          wait_frames = [wait_frames - 3, 5].max if block_count % blocks_per_level == 0
          delete_full_rows
          y = 0
          x = 4
          curr_block = next_block
          next_block = Block.new(random_block, 0)
          render_next_block(win, next_block)
          unless can_move_to?(curr_block, x, y)
            win.setpos(8, 0)
            # win << "                        "
            win.setpos(9, 0)
            # win << "       GAME OVER        "
            win.setpos(10, 0)
            # win << "                        "
            win.setpos(11, 0)
            # win << "   PRESS ENTER TO EXIT  "
            win.setpos(12, 0)
            # win << "                        "
            loop {
              c = win.getch
              break if c == 10 || c == 13
            }
            break
          end
        end
        start_frame = current_frame
      end
    end
  end

  def can_drop?(block, x, y)
    r = can_move_to?(block, x, y+1)
    r
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
end

begin
  PsxMruby.print_msg 'Starting the game.'
  MainGame.new.main_loop
rescue => ex
  # Note backtrace is only available when you pass -g to mrbc
  PsxMruby.print_msg ex.inspect
  PsxMruby.print_msg ex.backtrace.inspect
  raise ex
end
