#!/usr/bin/env ruby
# -*- coding: utf-8 -*-

def win_rate win, lose, draw
  (win.to_f*2 + draw) / ((win + lose + draw) * 2)
end

def confidence_interval win, lose, draw
  # 95% 信頼区間: 1.96
  # 99% 信頼区間: 2.58
  games = win + lose + draw
  win_r = win_rate(win, lose, draw)
  1.96*(win_r*(1.0 - win_r)/games)**0.5
end

def elo wr
  return 0.0 if (wr <= 0.0)
  rating = 400 * -Math::log10(1/(wr) - 1)
end

def main argv
  if argv.size < 1
    puts "USAGE: " + __FILE__ + " <selfplay_log_files> ..."
    puts "This program prints sum of selfplay result"
    exit
  end

  (win,lose,draw) = [0, 0, 0]
  game_num = 0
  argv.each do |glob_str|
    Dir.glob(glob_str).each do |file|
      result_line = `tail -n 1 #{file}`
      result_array = result_line.gsub("/"," ").split(" ")
      if result_array.size == 15
        win  += result_array[4].to_i
        lose += result_array[6].to_i
        draw += result_array[8].to_i
        game_num += result_array[2].to_i
      else
        puts "#{file} is NG."
      end
    end
  end
  printf "(%5d/%5d) W: %5d L: %5d D: %5d  WR: %6.2f +-%6.2f ELO: %3d\n", win + lose + draw, game_num, win, lose, draw, (win_rate(win, lose, draw)*100).round(2), (confidence_interval(win, lose, draw)*100).round(2), elo(win_rate(win, lose, draw)).to_i
end

main ARGV
