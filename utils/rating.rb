#!/usr/bin/env ruby

if(ARGV.size != 2 && ARGV.size != 3)
  puts "USAGE: " + __FILE__ + " <target win> <base win> <draw num>"
  puts "This program prints a target rating against base"
  exit
end

t = ARGV[0].to_i
b = ARGV[1].to_i
d = 0
if(ARGV.size == 3)
  d = ARGV[2].to_i
end

game = t + b + d
bf = b + d.to_f/2

rating = 400 * Math::log10(1/(bf/game) - 1)

puts rating
