#!/usr/bin/env ruby
# -*- coding: utf-8 -*-

# 自己対局を行う為のスクリプト
# 千日手判定は engine の key に頼っている。
# 反則手は指さない前提で作っている。

require 'open3'
require 'timeout'
require 'thread'

TimeoutSec = 60 # usi コマンドに usiok を返すまでの制限時間

class USIEngine
  attr_accessor :stdin, :stdout, :path
  def initialize engine_path
    @path = engine_path
    begin
      @stdin, @stdout = Open3.popen2e(engine_path)
    rescue => error
      puts error
      exit
    end
  end
end

class GameManager
  def initialize argv
    thread_num = 1
    thread_num = argv[2].to_i if 2 < argv.size
    @game_num = 3000
    @game_num = argv[3].to_i if 3 < argv.size
    @movetime = 100.to_s
    @movetime = argv[4] if 4 < argv.size

    @win = [0, 0]
    @draw = 0
    @game_index = 0
    #@mutex = Mutex.new

    enginess = []
    thread_num.times do
      enginess << [USIEngine.new(argv[0]), USIEngine.new(argv[1])]
    end
    threads = []
    thread_num.times do |i|
      threads << Thread.new { selfplay(enginess[i]) }
    end
    threads.each do |t|
      t.join
    end
  end

  def usiok? engines
    engines.each do |engine|
      engine.stdin.puts "usi"
    end
    engines.each do |engine|
      begin
        Timeout::timeout(TimeoutSec) do
          loop do
            line = engine.stdout.readline.chomp
            break if (line == "usiok")
          end
        end
      rescue Timeout::Error
        puts "#{engine.path} is not usi engine."
        exit
      end
    end
  end

  def setoption engines
    engines.each do |engine|
      engine.stdin.puts "setoption name Threads value 1"
      engine.stdin.puts "setoption name Byoyomi_Margin value 0"
      engine.stdin.puts "setoption name USI_Hash value 256"
    end
  end

  def selfplay_one engines, first_player
    key_hash = Hash.new
    moves = ""
    engines.each do |engine|
      engine.stdin.puts "usinewgame"
    end
    turn = first_player
    ply = 1
    loop do
      if ply == 257
        @draw += 1
        return
      end
      position = "position startpos moves " + moves
      engines[turn].stdin.puts position
      engines[turn].stdin.puts "key"
      key = engines[turn].stdout.readline.chomp.to_i
      if key_hash[key] == nil
        key_hash[key] = 0
      else
        key_hash[key] += 1
        if (key_hash[key] == 4)
          # sennichite
          @draw += 1
          return
        end
      end
      engines[turn].stdin.puts "go byoyomi " + @movetime
      loop do
        line = engines[turn].stdout.readline.chomp
        if line.include?("bestmove")
          move = line.split[1] # 次の手の文字列
          if (move == "resign")
            @win[1 ^ turn] += 1
            return
          end
          moves += move + " "
          break
        end
      end
      turn = 1 ^ turn
      ply += 1
    end
  end

  def selfplay engines
    usiok?(engines)
    setoption(engines)
    first_player = 0
    while @game_index < @game_num
      @game_index += 1
      selfplay_one(engines, first_player)
      first_player = 1 ^ first_player
      w, l, d = [@win[0], @win[1], @draw]
      win_r = win_rate(w, l, d)
      conf_interval = confidence_interval(w, l, d)
      printf "(%5d/%5d) W: %5d L: %5d D: %5d  WR: %6.2f +-%6.2f\n", w + l + d, @game_num, w, l, d, (win_r*100).round(2), (conf_interval*100).round(2)
    end
  end

  def win_rate win, lose, draw
    (win.to_f*2 + draw) / ((win + lose + draw) * 2)
  end

  def confidence_interval win, lose, draw
    # 95%: 1.96
    # 99%: 2.58
    games = win + lose + draw
    win_r = win_rate(win, lose, draw)
    1.96*(win_r*(1.0 - win_r)/games)**0.5
  end

  def elo wr
    return 0.0 if (wr <= 0.0)
    rating = 400 * Math::log10(1/(wr) - 1)
  end
end

def main argv
  if argv.size < 2
    puts "USAGE: " + __FILE__ + " <engine1> <engine2> <thread num> <game num> <movetime>"
    puts "This program does selfplay matches."
    exit
  end
  gameManager = GameManager.new(argv)
end

main ARGV
