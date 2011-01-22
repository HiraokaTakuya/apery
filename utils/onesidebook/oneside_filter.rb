#!/usr/bin/env ruby
# -*- coding: utf-8 -*-

MinBlackThresh = 70
MaxWhiteThresh = 30
#MinBlackThresh = 0
#MaxWhiteThresh = 0

Strongs = [
           "ponanza_expt",
           "ponanza-990XEE",
           "NineDayFever_XeonE5-2690_16c",
           "maybe_tomorrow",
           "tsutsukana",
           "PuppetMaster",
           "ttkn_eval",
           "YssF_6t_x1",
           "gpsfish_XeonX5470_8c",
           "gpsfish_XeonX5680_12c",
           "BlunderXX-WCSC23",
           "Apery_2700K_4c",
           "Apery_3930K_6c",
           "Apery_Windows7_2700K_4c",
           "Apery_WCSC23",
           "jidaiokure",
          ]

WaitMove = 0
WaitComment = 1
Black = 0
White = 1
def opposite_color c
  return c ^ 1
end

def interger? str
  Integer(str)
  return true
rescue ArgumentError
  return false
end

def include_strongs? strongs, line
  strongs.each do |strong_name|
    if line.include?(strong_name)
      return true
    end
  end
  return false
end

def toryo? line
  if line.include?("summary:toryo")
    return true
  end
  return false
end

def black_win? line
  if line.include?("summary:toryo")
    winorlose = line.split[-1]
    if winorlose == "lose"
      return true
    end
  end
  return false
end

def white_win? line
  if line.include?("summary:toryo")
    winorlose = line.split[-1]
    if winorlose == "win"
      return true
    end
  end
  return false
end

black_oneside_win_count = 0
white_oneside_win_count = 0
#Dir.glob("2014/wdoor.c.u-tokyo.ac.jp/shogi/x/2014/??/??/*.csa").each do |filename|
Dir.glob("2013/wdoor.c.u-tokyo.ac.jp/shogi/x/2013/??/??/*.csa").each do |filename|
#Dir.glob("2012/2012/*.csa").each do |filename|
  File.open(filename) do |file|
    next if !(file.gets) # v2
    next if !(black_name = file.gets)
    next if !(white_name = file.gets)
    black_name.chomp!
    white_name.chomp!
    black_name.slice!(0..1)
    white_name.slice!(0..1)

    black_is_strong_flag = include_strongs?(Strongs, black_name)
    white_is_strong_flag = include_strongs?(Strongs, white_name)
    next if !black_is_strong_flag && !white_is_strong_flag
    black_win_flag = false
    white_win_flag = false
    strong_win_flag = false
    toryo_flag = false
    onesidegame = false
    records = ""
    state = WaitMove
    minscore = 10000000
    maxscore = -10000000
    black_comment_exist_flag = false
    white_comment_exist_flag = false
    nextcolor = Black

    while line = file.gets
      if toryo?(line)
        toryo_flag = true
        if black_win?(line)
          black_win_flag = true
          if black_is_strong_flag
            strong_win_flag = true
          end
        end
        if white_win?(line)
          white_win_flag = true
          if white_is_strong_flag
            strong_win_flag = true
          end
        end
      end

      if /^[\+\-]\d\d\d\d[A-Z][A-Z]$/ =~ line
        records += line[1..6]
        time = file.gets # time
        if !(/^T\d+$/ =~ time)
          STDERR.puts "error time: #{time}"
          STDERR.puts line
          break # toryo_flag は false の状態で break するから、棋譜は使用されない。
        end
        state = WaitComment
        nextcolor = opposite_color(nextcolor)
      elsif state == WaitComment && line[0..3] == "'** "
        if !interger?(line.split[1])
          break # 相手の評価値も重要な指標なので、相手が評価値を出力しないなら棋譜を使わない。
        end
        score = line.split[1].to_i
        # 評価値 0 は定跡や、upperbound, lowerbound などの要因で出力されるので、無視する。
        if score != 0 && score < minscore
          minscore = score
        elsif score != 0 && maxscore < score
          maxscore = score
        end
        if nextcolor == Black
          white_comment_exist_flag = true # next が Black なので、この評価値のコメントは White のもの。
        else
          black_comment_exist_flag = true # next が White なので、この評価値のコメントは Black のもの。
        end
        state = WaitMove
      elsif state == WaitComment
        #STDERR.puts "error comment: no evaluate comment"
        break
      end
    end

    onesidegame = (black_win_flag && MinBlackThresh < minscore) || (white_win_flag && maxscore < MaxWhiteThresh)
    tesu = records.size / 6
    if toryo_flag && strong_win_flag && onesidegame && 30 < tesu && black_comment_exist_flag && white_comment_exist_flag
      if black_win_flag
        black_oneside_win_count += 1
      else
        white_oneside_win_count += 1
      end
      num = black_oneside_win_count + white_oneside_win_count
      date = filename.gsub(/.*\+/, "").gsub(".csa", "").chomp
      winstate = 1
      winstate = 2 if white_win_flag

      senkei = "?"
      arr = [minscore, maxscore]
      puts "#{num} #{date} #{black_name} #{white_name} #{winstate} #{tesu} #{senkei} #{arr[winstate-1]}"
      puts records
    end
  end
end

STDERR.puts "black_oneside_count = #{black_oneside_win_count}"
STDERR.puts "white_oneside_count = #{white_oneside_win_count}"
