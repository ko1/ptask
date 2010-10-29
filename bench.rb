#!/usr/bin/ruby

file = ARGV.shift || raise
limit = (ARGV.shift || 10).to_i
#p limit
#p file

require 'benchmark'
(1..limit).times{|i|
  cmd = "#{file} #{i}"
  real = Benchmark.measure{
    `#{cmd}`
  }.real
  puts "#{i}\t#{real}"
}
