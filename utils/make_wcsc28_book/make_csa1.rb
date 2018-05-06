#!/usr/bin/env ruby
# -*- coding: utf-8 -*-

Strongs = [
  "elmo",
  "Hefeweizen",
  "the end of genesis T.N.K.evolution turbo type D"
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
  if line.include?("summary:toryo") || line.include?("%TORYO")
    return true
  end
  return false
end

def timeup? line
  if line.include?("summary:time up")
    return true
  end
  return false
end

def black_win? next_color
  next_color != Black
end

def white_win? next_color
  next_color != White
end

def main
  kifu_num = 0
  Dir.glob("kifu/utf8_*.csa").each do |filename|
    File.open(filename) do |file|
      next if !(file.gets) # v2
      next if !(black_name = file.gets)
      next if !(white_name = file.gets)
      next if !(file.gets) # START_TIME
      next if !(file.gets) # PI
      next if !(file.gets) # +
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
      timeup_flag = false
      records = ""
      state = WaitMove
      minscore = 10000000
      maxscore = -10000000
      black_comment_exist_flag = false
      white_comment_exist_flag = false
      nextcolor = Black

      while line = file.gets
        if toryo?(line) || timeup?(line)
          toryo_flag = toryo?(line)
          timeup_flag = timeup?(line)
          if black_win?(nextcolor)
            black_win_flag = true
            if black_is_strong_flag
              strong_win_flag = true
            end
          end
          if white_win?(nextcolor)
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
            #break # 評価値出ないソフトは省いても良いか。
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
          #break
          state = WaitMove
        end
      end

      tesu = records.size / 6
      if (toryo_flag || (timeup_flag && 100 < tesu)) && strong_win_flag && 30 < tesu #&& black_comment_exist_flag && white_comment_exist_flag
        kifu_num += 1
        date = filename.gsub(/.*\+/, "").gsub(".csa", "").chomp
        winstate = 1
        winstate = 2 if white_win_flag

        senkei = "?"
        arr = [minscore, maxscore]
        puts "#{kifu_num} #{date} #{black_name} #{white_name} #{winstate} #{tesu} #{senkei} #{arr[winstate-1]}"
        puts records
      end
    end
  end
end

main
