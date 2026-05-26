#pragma once
#include "discord-config.h"
#include "discord-rpc.h"

namespace discord_utils
{
	struct presence_snapshot_t
	{
		qstring details;
		qstring state;
		qstring large_image_key;
		qstring large_image_text;
		qstring small_image_key;
		qstring small_image_text;
		int64_t start_timestamp = 0;
	};

	static const char* presence_text_or_null( const char* value ) {
		return value != NULL && value[ 0 ] != '\0' ? value : NULL;
	}

	static qstring presence_text_or_empty( const char* value ) {
		return value != NULL ? value : "";
	}

	static bool presence_snapshots_equal( const presence_snapshot_t& lhs, const presence_snapshot_t& rhs ) {
		return lhs.details == rhs.details
			&& lhs.state == rhs.state
			&& lhs.large_image_key == rhs.large_image_key
			&& lhs.large_image_text == rhs.large_image_text
			&& lhs.small_image_key == rhs.small_image_key
			&& lhs.small_image_text == rhs.small_image_text
			&& lhs.start_timestamp == rhs.start_timestamp;
	}

	static void handle_discord_ready( ) {

		if ( g_options.output_type >= ( int )output_type::errors_results_and_interim_steps && g_options.output_enabled ) {
			msg( "Discord: ready\n" );
		}
	}

	static void handle_discord_disconnected( int errcode, const char* message ) {

		if ( g_options.output_type >= ( int )output_type::errors_only && g_options.output_enabled ) {
			msg( "Discord: disconnected (%d: %s)\n", errcode, message );
		}
	}

	static void handle_discord_error( int errcode, const char* message ) {

		if ( g_options.output_type >= ( int )output_type::errors_only && g_options.output_enabled ) {
			msg( "Discord: error (%d: %s)\n", errcode, message );
		}
	}

	static void discord_init( const char* app_id ) {

		DiscordEventHandlers handlers;
		memset( &handlers, 0, sizeof( handlers ) );

		handlers.ready		  = handle_discord_ready;
		handlers.disconnected = handle_discord_disconnected;
		handlers.errored	      = handle_discord_error;

		Discord_Initialize( app_id, &handlers, 1, NULL );
	}

	static bool update_discord_presence( int64_t start_time, bool deduplicate = false ) {

		static presence_snapshot_t last_presence;
		static bool has_last_presence = false;

		if ( g_options.rpc_enabled ) {

			DiscordRichPresence rpc;
			memset( &rpc, 0, sizeof( rpc ) );

			( g_options.filename_enabled ) ? rpc.details = ida_utils::get_current_filename( ) : rpc.details = "";

			char state[ MAXSTR ]; // heh *teleports behind you*
			state[ 0 ] = '\0';

			const char* current_function = ida_utils::get_current_function_name( );

			if ( current_function != NULL && current_function[ 0 ] != '\0' && strcmp( current_function, "(null)" ) != 0 ) {
				
				if ( g_options.address_enabled ) {

					switch ( g_options.address_type )
					{
						case ( int )address_type::function_base_address:

							qsnprintf( state, sizeof( state ) - 1, "Reversing %s ( 0x%a )", current_function, ida_utils::get_current_function_start_address( ) );
							break;

						case ( int )address_type::current_cursor_address:
							qsnprintf( state, sizeof( state ) - 1, "Reversing %s ( 0x%a )", current_function, ida_utils::get_current_cursor_address( ) );
							break;

						default:
							qsnprintf( state, sizeof( state ) - 1, "Reversing %s", current_function );
							break;
					}
				}

				else if ( !g_options.address_enabled ) {

					qsnprintf( state, sizeof( state ) - 1, "Reversing %s", current_function );
				}
			}
			else {

				qsnprintf( state, sizeof( state ) - 1, "Idle" );
			}
			
			( g_options.functionname_enabled ) ? rpc.state = state : rpc.state = "";

			( g_options.timeelapsed_enabled ) ? rpc.startTimestamp = start_time : rpc.startTimestamp = 0;

			rpc.largeImageKey = presence_text_or_null( discord_config::large_image_key );
			rpc.largeImageText = presence_text_or_null( discord_config::large_image_text );
			rpc.smallImageKey = presence_text_or_null( discord_config::small_image_key );
			rpc.smallImageText = presence_text_or_null( discord_config::small_image_text );

			presence_snapshot_t current_presence;
			current_presence.details = presence_text_or_empty( rpc.details );
			current_presence.state = presence_text_or_empty( rpc.state );
			current_presence.large_image_key = presence_text_or_empty( rpc.largeImageKey );
			current_presence.large_image_text = presence_text_or_empty( rpc.largeImageText );
			current_presence.small_image_key = presence_text_or_empty( rpc.smallImageKey );
			current_presence.small_image_text = presence_text_or_empty( rpc.smallImageText );
			current_presence.start_timestamp = rpc.startTimestamp;

			if ( deduplicate
			  && has_last_presence
			  && presence_snapshots_equal( current_presence, last_presence ) ) {
				return false;
			}

			Discord_UpdatePresence( &rpc );
			last_presence = current_presence;
			has_last_presence = true;
			return true;
		}

		else if ( !g_options.rpc_enabled ) {

			Discord_ClearPresence( );
			has_last_presence = false;

			return true;
		}

		return false;
	}
}
