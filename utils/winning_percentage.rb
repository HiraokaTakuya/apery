#!/usr/bin/env ruby

if(ARGV.size != 1)
  puts "USAGE: " + __FILE__ + " <rating difference>"
  puts "This program prints a winning percentage."
  exit
end

rd = ARGV[0].to_f

wp = 100/(1+10**(-rd/400))

puts wp
