/**
 * @file coroutine_test5.cpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-01-07
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <chrono>
#include <condition_variable>
#include <coroutine>
#include <deque>
#include <functional>
#include <future>
#include <iostream>
#include <list>
#include <mutex>
#include <thread>
#include <utility>

#include "coropile.hpp"

int add_1( int v )
{
	return v + 1;
}

template <class Rep, class Period>
coropile_awaitable<void> AsyncSleeper( const std::chrono::duration<Rep, Period>& rel_time )
{
	coropile_debug_print( std::string( "AsyncSleeper start" ) );
	co_await std::async( std::launch::async, [rel_time]() -> void {
        std::this_thread::sleep_for(rel_time);
        return; } );
	coropile_debug_print( std::string( "AsyncSleeper done" ) );
	co_return;
}

coropile_awaitable<int> TestAsync3( int v )
{
	coropile_debug_print( std::string( "TestAsync3 -> " ) + std::to_string( v ) );
	co_await AsyncSleeper( std::chrono::seconds( 2 ) );
	int n = add_1( v + 1 );

	coropile_debug_print( std::string( "TestAsync3 done -> " ) + std::to_string( v ) );
	co_return n;
}

coropile_awaitable<int> TestAsync2( int v )
{
	coropile_debug_print( std::string( "TestAsync2 -> " ) + std::to_string( v ) );
	int n = co_await TestAsync3( v + 1 );

	coropile_debug_print( std::string( "TestAsync2 done -> " ) + std::to_string( v ) );
	co_return n;
}

coropile_awaitable<int> TestAsync1( int v )
{
	coropile_debug_print( std::string( "TestAsync1 -> " ) + std::to_string( v ) );
	int n = co_await TestAsync2( v + 1 );

	coropile_debug_print( std::string( "TestAsync1 done -> " ) + std::to_string( v ) );
	co_return n;
}

int main()
{
	std::cout << "#1: main 2 - tid:" << std::this_thread::get_id() << std::endl;

	auto f2 = TestAsync1( 1 );

	std::cout << "#2: main 2 - tid:" << std::this_thread::get_id() << std::endl;

	while ( !f2.is_completed() ) {
		f2.wait_notifier();
		f2.call_resume();   // TestAsync1()のinitial suspendに対する再開指示
		std::cout << "#3: main 2 - tid:" << std::this_thread::get_id() << std::endl;
	}

	std::cout << "#F: main 2 - tid:" << std::this_thread::get_id() << std::endl
			  << " " << f2.get_return_value() << std::endl;

	return EXIT_SUCCESS;
}
