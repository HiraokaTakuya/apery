#!/usr/bin/env ruby
# -*- coding: utf-8 -*-

# 自己対局を行う為のスクリプト
# 千日手判定は engine の key に頼っている。
# 反則手は指さない前提で作っている。

require 'open3'
require 'timeout'
require 'thread'

TimeoutSec = 60 # isready コマンドに readyok を返すまでの制限時間

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
    @game_num  = 3000
    @movetime  = 100.to_s
    @win       = [0, 0]
    @draw      = 0

    @file      = File.open(argv[2], "a") if 2 < argv.size && argv[2] != '-'
    thread_num = argv[3].to_i            if 3 < argv.size
    @game_num  = argv[4].to_i            if 4 < argv.size
    @movetime  = argv[5]                 if 5 < argv.size
    @win[0]    = argv[6].to_i            if 6 < argv.size
    @win[1]    = argv[7].to_i            if 7 < argv.size
    @draw      = argv[8].to_i            if 8 < argv.size

    @game_index = @win[0] + @win[1] + @draw

    enginess = []
    thread_num.times do
      enginess << [USIEngine.new(argv[0]), USIEngine.new(argv[1])]
    end
    threads = []
    thread_num.times do |i|
      threads << Thread.new { selfplay(enginess[i], i%2) }
    end
    threads.each do |t|
      t.join
    end
  end

  def isready engines
    engines.each do |engine|
      engine.stdin.puts "isready"
    end
    engines.each do |engine|
      begin
        Timeout::timeout(TimeoutSec) do
          loop do
            line = engine.stdout.readline.chomp
            break if (line == "readyok")
          end
        end
      rescue Timeout::Error
        puts "#{engine.path} is not usi engine."
        exit
      end
    end
  end

  # 対局時のエンジンの設定。必要であれば設定を変えて実験する。秒読みはスクリプト起動時の引数で設定出来る。
  def setoption engines
    engines.each do |engine|
      engine.stdin.puts "setoption name Threads value 1"
      engine.stdin.puts "setoption name Byoyomi_Margin value 0"
      engine.stdin.puts "setoption name USI_Hash value 256"
    end
  end

  def write_record sfen, moves
    if @file
      @file.write "position " + sfen + " moves " + moves + "\n"
      @file.flush
    end
  end

  def selfplay_one engines, first_player
    key_hash = Hash.new
    moves = ""
    isready(engines)
    turn = first_player
    ply = 1
    loop do
      if ply == 257
        @draw += 1
        write_record "startpos", moves
        return
      end
      position = "position startpos moves " + moves
      engines[turn].stdin.puts position
      engines[turn].stdin.puts "key"
      key = engines[turn].stdout.readline.chomp.to_i
      if key_hash[key] == nil
        key_hash[key] = 1
      else
        key_hash[key] += 1
        if (key_hash[key] == 4)
          # 千日手
          @draw += 1
          write_record "startpos", moves
          return
        end
      end
      engines[turn].stdin.puts "go byoyomi " + @movetime
      loop do
        line = engines[turn].stdout.readline.chomp
        if line.include?("bestmove")
          move = line.split[1] # 次の手の文字列
          if move == "resign"
            @win[1 ^ turn] += 1
            write_record "startpos", moves
            return
          elsif move == "win"
            @win[turn] += 1
            write_record "startpos", moves
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

  def selfplay engines, first_player
    setoption(engines)
    while @game_index < @game_num
      @game_index += 1
      selfplay_one(engines, first_player)
      first_player = 1 ^ first_player
      w, l, d = [@win[0], @win[1], @draw]
      win_r = win_rate(w, l, d)
      conf_interval = confidence_interval(w, l, d)
      printf "(%5d/%5d) W: %5d L: %5d D: %5d  WR: %6.2f +-%6.2f\n", w + l + d, @game_num, w, l, d, (win_r*100).round(2), (conf_interval*100).round(2)
      STDOUT.flush
    end
  end

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
    rating = 400 * Math::log10(1/(wr) - 1)
  end
end

def main argv
  if argv.size < 2
    puts "USAGE: " + __FILE__ + " <engine1> <engine2> <output kifu file ('-' is no output)> <thread num> <game num> <movetime> <init win> <init lose> <init draw>"
    puts "This program does selfplay matches."
    exit
  end
  gameManager = GameManager.new(argv)
end

main ARGV
