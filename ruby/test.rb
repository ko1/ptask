ENV['RUBY_MAX_WORKER_NUM'] = '1'
require './task'

nmax = 3
pmax = 10

def test nmax, pmax, method, *args
  nmax.times{
      (1..pmax).map{|i|
        TaskTest.send(method, *args)
    }.each{|e|
      e.inspect
    }
  }
end

require 'benchmark'

$sleep = false
$empty = false

Benchmark.bm{|x|
  x.report('sleep(1)'){
    test 3, 10, :sleep, 1
  } if $sleep
  x.report('empty()'){
    test 1000, 1000, :empty
  } if $empty
  20.times{|i|
    n = i * 500
    x.report("repeat(#{n})"){
      test 1000, 1000, :repeat, n
    }
  }
}

