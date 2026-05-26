#pragma once

#define STRINGIFY_IDA_INPUT_FIELD( field_identifier_str, field_id ) field_identifier_str #field_id
#define DROPDOWN_FIELD( field_id ) STRINGIFY_IDA_INPUT_FIELD( "b", field_id ) // lowercase b is dropdown see https://www.hex-rays.com/products/ida/support/sdkdoc/group___f_o_r_m___c.html for reference

namespace ida_utils
{
	inline bool is_idb_loaded( ) {

		return ( strlen( get_path( PATH_TYPE_IDB ) ) != 0 );
	}

	inline const char* get_current_filename( ) {

		static char filename[ MAXSTR ];
		filename[ 0 ] = '\0';

		ssize_t read_size = get_root_filename( filename, sizeof( filename ) );

		if ( read_size > 0 ) {
			return filename;
		}

		return "";
	}

	inline const char* get_current_processor_module( ) {

		static char processor_module[ MAXSTR ];
		processor_module[ 0 ] = '\0';

		if ( get_idp_name( processor_module, sizeof( processor_module ) ) != NULL ) {
			return processor_module;
		}

		return "";
	}

	inline ea_t get_current_cursor_address( ) {

		ea_t cur_addr = get_screen_ea( );

		if ( cur_addr != BADADDR && is_code( get_flags( cur_addr ) ) ) {
			return cur_addr;
		}

		return BADADDR;
	}

	inline ea_t get_current_function_start_address( ) {

		ea_t cur_addr = get_current_cursor_address( );

		if ( cur_addr == BADADDR ) {
			return BADADDR;
		}

		func_t *func = get_func( cur_addr );

		if ( func != NULL ) {
			return func->start_ea;
		}

		return BADADDR;
	}

	inline const char* get_current_function_name( ) {

		ea_t cur_addr = get_current_cursor_address( );

		if ( cur_addr == BADADDR ) {
			return "";
		}

		func_t *func = get_func( cur_addr );

		ssize_t size_read = 0;

		if ( func != NULL ) {

			static qstring func_name;
			func_name.clear( );

			size_read = get_func_name( &func_name, func->start_ea );

			if ( size_read > 0 ) {
				return func_name.c_str( );
			}
		}

		return "";
	}
}
