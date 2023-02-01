/**
 * @file coropile.hpp
 * @author PFA03027@nifty.com
 * @brief Return type templates to support multiple calls of coroutines
 * @version 0.1
 * @date 2023-01-29
 *
 * コルーチンの多重呼び出しをサポートするための戻り値型のテンプレート
 *
 * @copyright Copyright (c) 2023, PFA03027@nifty.com
 *
 */

#ifndef COROPILE_HPP_
#define COROPILE_HPP_

#include <chrono>
#include <coroutine>
#include <functional>
#include <future>
#include <list>
#include <semaphore>
#include <string>
#include <thread>
#include <utility>

#ifdef COROPILE_INTERNAL_DEBUG_LOG
#include <iostream>

inline void coropile_debug_print( std::string debug_log_str )
{
	std::cout << "[tid:" << std::this_thread::get_id() << "] " << debug_log_str << std::endl;
	return;
}
#else
inline void coropile_debug_print( std::string debug_log_str )
{
	return;
}
#endif

/**
 * @brief コルーチンスタックを構成するための1スタック分の情報
 *
 */
struct coropile_hdlobj {
	std::coroutine_handle<>                     coroh_;         //!< コルーチンハンドル
	std::shared_ptr<std::counting_semaphore<1>> sp_notifier_;   //!< 更新通知受信用オブジェクト

	coropile_hdlobj( std::coroutine_handle<> coroh_arg, std::shared_ptr<std::counting_semaphore<1>> sp_notifier_arg )
	  : coroh_( coroh_arg )
	  , sp_notifier_( sp_notifier_arg )
	{
	}
};

using coropile_coroutine_stack = std::list<coropile_hdlobj>;   //!< コルーチンスタック

template <typename ReturnValueType>
class coropile_awaitable;
template <typename ReturnValueType>
struct coropile_awaiter;
template <typename ReturnValueType>
struct coropile_awaiter_future;

/**
 * @brief コルーチン側で保持されるpromiseオブジェクトの型
 *
 * @tparam ReturnValueType
 */
template <typename ReturnValueType>
struct coropile_promise_type {
	coropile_promise_type( void );

	coropile_awaitable<ReturnValueType> get_return_object();
	std::suspend_never                  initial_suspend();
	std::suspend_always                 final_suspend() noexcept( true );
	void                                return_value( ReturnValueType value );
	void                                unhandled_exception();
	template <typename ValueType2>
	coropile_awaiter<ValueType2> await_transform( coropile_awaitable<ValueType2> f )
	{
		// 呼び出しされているコルーチンハンドルのチェーンを結合し、共有する。
		f.concat_coropile_coroutine_stack( sp_crc_stack_ );
		coropile_debug_print( std::string( "#1: coropile_promise_type<ReturnValueType>::await_transform( coropile_awaitable<ValueType2> f ) -> " ) + std::to_string( sp_crc_stack_->size() ) );

		return coropile_awaiter<ValueType2> { std::move( f ) };
	}
	template <typename ValueType2>
	coropile_awaiter_future<ValueType2> await_transform( std::future<ValueType2> f )
	{
		coropile_debug_print( std::string( "#1: coropile_promise_type<ReturnValueType>::await_transform( std::future<ValueType2> f )" ) + std::to_string( sp_crc_stack_->size() ) );
		return coropile_awaiter_future<ValueType2> { std::move( f ), sp_notifier_ };
	}

protected:
	std::promise<ReturnValueType>               value_;          //!< コルーチンの戻り値を設定するためのpromise
	std::coroutine_handle<>                     my_coroh_;       //!< コルーチンハンドル
	std::shared_ptr<std::counting_semaphore<1>> sp_notifier_;    //!< コルーチンが完了待ちしている処理の完了通知送信用のpromise
	std::shared_ptr<coropile_coroutine_stack>   sp_crc_stack_;   //!< コルーチンスタック
};

/////////// void型の場合の明示的特殊化クラス ////////////
template <>
struct coropile_promise_type<void> {
	coropile_promise_type( void );

	coropile_awaitable<void> get_return_object();
	std::suspend_never       initial_suspend();
	std::suspend_always      final_suspend() noexcept( true );
	void                     return_void();
	void                     unhandled_exception();
	template <typename ValueType2>
	coropile_awaiter<ValueType2> await_transform( coropile_awaitable<ValueType2> f )
	{
		// 呼び出しされているコルーチンハンドルのチェーンを結合し、共有する。
		f.concat_coropile_coroutine_stack( sp_crc_stack_ );
		coropile_debug_print( std::string( "#1: coropile_promise_type<void>::await_transform( coropile_awaitable<ValueType2> f ) -> " ) + std::to_string( sp_crc_stack_->size() ) );

		return coropile_awaiter<ValueType2> { std::move( f ) };
	}

	template <typename ValueType2>
	coropile_awaiter_future<ValueType2> await_transform( std::future<ValueType2> f )
	{
		coropile_debug_print( std::string( "#1: coropile_promise_type<void>::await_transform( std::future<ValueType2> f )" ) + std::to_string( sp_crc_stack_->size() ) );
		return coropile_awaiter_future<ValueType2> { std::move( f ), sp_notifier_ };
	}

protected:
	std::promise<void>                          value_;          //!< コルーチンの戻り値を設定するためのpromise
	std::coroutine_handle<>                     my_coroh_;       //!< コルーチンハンドル
	std::shared_ptr<std::counting_semaphore<1>> sp_notifier_;    //!< コルーチンが完了待ちしている処理の完了通知送信用のpromise
	std::shared_ptr<coropile_coroutine_stack>   sp_crc_stack_;   //!< コルーチンスタック
};

/**
 * @brief コルーチンの戻り値型
 *
 * @tparam ReturnValueType コルーチンの関数としての戻り値型
 */
template <typename ReturnValueType>
class coropile_awaitable {
public:
	using promise_type = coropile_promise_type<ReturnValueType>;

	coropile_awaitable( coropile_awaitable&& rhs )
	  : future_( std::move( rhs.future_ ) )
	  , coroh_( rhs.coroh_ )
	  , sp_crc_stack_( std::move( rhs.sp_crc_stack_ ) )
	{
		rhs.coroh_ = nullptr;
	}

	coropile_awaitable& operator=( coropile_awaitable&& rhs )
	{
		future_       = std::move( rhs.future_ );
		coroh_        = rhs.coroh_;
		sp_crc_stack_ = std::move( rhs.sp_crc_stack_ );

		rhs.coroh_ = nullptr;
	}

	~coropile_awaitable()
	{
		// std::cout << "#1: ~coropile_awaitable tid:" << std::this_thread::get_id() << std::endl;
		if ( coroh_ != nullptr ) {
			coropile_debug_print( std::string( "#2: ~coropile_awaitable calls destroy()" ) );
			coroh_.destroy();
			coroh_ = nullptr;
		}
	}

	/**
	 * @brief コルーチンが再開可能かをチェックする
	 *
	 * @return true コルーチン再開可
	 * @return false コルーチン再開不可
	 */
	bool try_wait_notifier( void )
	{
		if ( is_completed() ) return true;

		if ( sp_crc_stack_ == nullptr ) {
			// TODO: 例外を投げるべき？
			return false;
		}

		if ( sp_crc_stack_->size() == 0 ) {
			// TODO: 例外を投げるべき？
			return false;
		}

		coropile_debug_print( std::string( "#1: try_wait_notifier 2" ) );
		if ( !sp_crc_stack_->front().sp_notifier_->try_acquire() ) {
			return false;
		}
		coropile_debug_print( std::string( "#2: try_wait_notifier 2" ) );

		return true;
	}

	/**
	 * @brief コルーチンが再開できるまで待つ
	 *
	 */
	void wait_notifier( void )
	{
		if ( is_completed() ) return;

		if ( sp_crc_stack_ == nullptr ) {
			// TODO: 例外を投げるべき？
			return;
		}

		if ( sp_crc_stack_->size() == 0 ) {
			// TODO: 例外を投げるべき？
			return;
		}

		coropile_debug_print( std::string( "#1: wait_notifier 2" ) );
		sp_crc_stack_->front().sp_notifier_->acquire();
		coropile_debug_print( std::string( "#2: wait_notifier 2" ) );

		return;
	}

	/**
	 * @brief コルーチンの実行を再開する。
	 *
	 * @return true 完了待ち処理が完了していて、コルーチンが再開できた。
	 * @return false 完了待ち処理が未完了。あるいはコルーチンスタックが空。
	 */
	bool call_resume( void )
	{
		if ( sp_crc_stack_ == nullptr ) {
			// TODO: 例外を投げるべき？
			return false;
		}

		if ( sp_crc_stack_->size() == 0 ) {
			// TODO: 例外を投げるべき？
			return false;
		}

		sp_crc_stack_->front().coroh_.resume();   // 完了待ち処理が完了したので、コルーチンを再開する。
		// std::cout << "#1: call_resume 2 - tid:" << std::this_thread::get_id() << std::endl;
		if ( sp_crc_stack_->front().coroh_.done() ) {
			// このハンドルは借り物で、本来の持ち主は、呼び出し元のコルーチンが受け取っているAwaitableオブジェクトである。
			// そのため、destroy()処理は、Awaitableオブジェクト側で実施しなければならない。
			// よって、ここでは借り物を返却する意味で、popするだけ。destroyは呼び出さない。
			sp_crc_stack_->pop_front();
			if ( sp_crc_stack_->size() != 0 ) {
				sp_crc_stack_->front().sp_notifier_->release();
			}
			coropile_debug_print( std::string( "#3: call_resume pop_front!!!!!" ) );
		}
		return true;
	}

	/**
	 * @brief Get the Return Value object
	 *
	 * コルーチンの処理完了によって得られる戻り値を得る。
	 *
	 * @return ReturnValueType
	 */
	ReturnValueType get_return_value()
	{
		return future_.get();
	}

	/**
	 * @brief コルーチンの処理が完了したかどうかを確認する。
	 *
	 * @return true コルーチンの処理が完了
	 * @return false コルーチンの処理が未完了
	 */
	bool is_completed() const
	{
		return future_.wait_for( std::chrono::seconds( 0 ) ) == std::future_status::ready;
	}

	explicit coropile_awaitable( std::future<ReturnValueType>&& future, std::coroutine_handle<> coroh_arg, std::shared_ptr<coropile_coroutine_stack> sp_crc_stack_arg )
	  : future_( std::move( future ) )
	  , coroh_( coroh_arg )
	  , sp_crc_stack_( sp_crc_stack_arg )
	{
	}

	/**
	 * @brief コルーチンスタックをつなげる
	 *
	 * @param sp_ccs_arg
	 */
	void concat_coropile_coroutine_stack( std::shared_ptr<coropile_coroutine_stack> sp_ccs_arg )
	{
		sp_ccs_arg->splice( sp_ccs_arg->begin(), std::move( *( sp_crc_stack_ ) ) );
		sp_crc_stack_ = sp_ccs_arg;
		return;
	}

protected:
private:
	coropile_awaitable( const coropile_awaitable& )            = delete;
	coropile_awaitable& operator=( const coropile_awaitable& ) = delete;

	std::future<ReturnValueType>              future_;
	std::coroutine_handle<>                   coroh_;
	std::shared_ptr<coropile_coroutine_stack> sp_crc_stack_;
};

template <typename ReturnValueType>
struct coropile_awaiter {
	explicit coropile_awaiter( coropile_awaitable<ReturnValueType> crp_awaitable_arg )
	  : future_( std::move( crp_awaitable_arg ) )
	{
	}

	bool await_ready() const
	{
		return future_.is_completed();
	}

	void await_suspend( std::coroutine_handle<> h )
	{
	}

	ReturnValueType await_resume()
	{
		coropile_debug_print( std::string( "coropile_awaiter<ReturnValueType>::await_resume - async" ) );
		if constexpr ( std::is_same<ReturnValueType, void>::value ) {
			future_.get_return_value();
			return;
		} else {
			ReturnValueType ans = future_.get_return_value();
			return ans;
		}
	}

private:
	coropile_awaitable<ReturnValueType> future_;
};

template <typename FUTURE_RET_VAL_TYPE>
struct coropile_awaiter_future {
	explicit coropile_awaiter_future( std::future<FUTURE_RET_VAL_TYPE> fu_arg, std::shared_ptr<std::counting_semaphore<1>> sp_arg )
	  : future_( std::move( fu_arg ) )
	  , notifier_( sp_arg )
	{
	}

	bool await_ready() const
	{
		return future_.wait_for( std::chrono::seconds( 0 ) ) == std::future_status::ready;
	}

	void await_suspend( std::coroutine_handle<> h )
	{
		coropile_debug_print( std::string( "coropile_awaiter_future<FUTURE_RET_VAL_TYPE>::await_suspend" ) );
		std::thread( [this]() {
                    future_.wait();
                    notifier_->release(); } )
			.detach();
	}

	FUTURE_RET_VAL_TYPE await_resume()
	{
		// std::cout << "Awaiter::await_resume - async tid:" << std::this_thread::get_id() << std::endl;
		if constexpr ( std::is_same<FUTURE_RET_VAL_TYPE, void>::value ) {
			coropile_debug_print( std::string( "coropile_awaiter_future<void>::await_resume" ) );
			future_.get();
			coropile_debug_print( std::string( "coropile_awaiter_future<void>::await_resume get value" ) );
			return;
		} else {
			coropile_debug_print( std::string( "coropile_awaiter_future<FUTURE_RET_VAL_TYPE>::await_resume" ) );
			FUTURE_RET_VAL_TYPE ans = future_.get();
			coropile_debug_print( std::string( "coropile_awaiter_future<FUTURE_RET_VAL_TYPE>::await_resume get value" ) );
			return ans;
		}
	}

private:
	std::future<FUTURE_RET_VAL_TYPE>            future_;
	std::shared_ptr<std::counting_semaphore<1>> notifier_;
};
////////////////////////////////////////////////////////////////////////////////////
template <typename ReturnValueType>
coropile_promise_type<ReturnValueType>::coropile_promise_type( void )
  : value_()
  , my_coroh_( std::coroutine_handle<typename coropile_awaitable<ReturnValueType>::promise_type>::from_promise( *this ) )
  , sp_notifier_( std::make_shared<std::counting_semaphore<1>>( 0 ) )
  , sp_crc_stack_( std::make_shared<coropile_coroutine_stack>() )
{
	sp_crc_stack_->emplace_front( my_coroh_, sp_notifier_ );
}

template <typename ReturnValueType>
std::suspend_never coropile_promise_type<ReturnValueType>::initial_suspend()
{
	return std::suspend_never {};
}

template <typename ReturnValueType>
std::suspend_always coropile_promise_type<ReturnValueType>::final_suspend() noexcept( true )
{
	return std::suspend_always {};
}

template <typename ReturnValueType>
void coropile_promise_type<ReturnValueType>::unhandled_exception()
{
	value_.set_exception( std::current_exception() );
}

template <typename ReturnValueType>
void coropile_promise_type<ReturnValueType>::return_value( ReturnValueType value )
{
	value_.set_value( value );
	return;
}

template <typename ReturnValueType>
coropile_awaitable<ReturnValueType> coropile_promise_type<ReturnValueType>::get_return_object()
{
	return coropile_awaitable( value_.get_future(), my_coroh_, sp_crc_stack_ );
}

/////////// void型の場合の明示的特殊化クラスに対応するメンバ関数の定義 ////////////
coropile_promise_type<void>::coropile_promise_type( void )
  : value_()
  , my_coroh_( std::coroutine_handle<typename coropile_awaitable<void>::promise_type>::from_promise( *this ) )
  , sp_notifier_( std::make_shared<std::counting_semaphore<1>>( 0 ) )
  , sp_crc_stack_( std::make_shared<coropile_coroutine_stack>() )
{
	sp_crc_stack_->emplace_front( my_coroh_, sp_notifier_ );
}

std::suspend_never coropile_promise_type<void>::initial_suspend()
{
	return std::suspend_never {};
}

std::suspend_always coropile_promise_type<void>::final_suspend() noexcept( true )
{
	return std::suspend_always {};
}

void coropile_promise_type<void>::unhandled_exception()
{
	value_.set_exception( std::current_exception() );
}

void coropile_promise_type<void>::return_void()
{
	value_.set_value();
	return;
}

coropile_awaitable<void> coropile_promise_type<void>::get_return_object()
{
	return coropile_awaitable( value_.get_future(), my_coroh_, sp_crc_stack_ );
}

#endif   // COROPILE_HPP_
