/******************************************************************************
 * Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

// examples/example_mdle.cpp
//
// Access the MDLE API and create MDLE files from existing mdl materials or functions.

#include <mi/mdl_sdk.h>
#include <iostream>

// Include code shared by all examples.
#include "example_shared.h"

int main( int /*argc*/, char* /*argv*/[])
{
    // Access the MDL SDK
    mi::base::Handle<mi::neuraylib::INeuray> neuray(load_and_get_ineuray());
    check_success(neuray.is_valid_interface());

    // Configure the MDL SDK
    configure(neuray.get());

    // Start the MDL SDK
    mi::Sint32 result = neuray->start();
    check_start_success(result);

    // Access the database and create a transaction.
    {
        mi::base::Handle<mi::neuraylib::IDatabase> database(
            neuray->get_api_component<mi::neuraylib::IDatabase>());
        mi::base::Handle<mi::neuraylib::IScope> scope(database->get_global_scope());
        mi::base::Handle<mi::neuraylib::ITransaction> transaction(scope->create_transaction());

        mi::base::Handle<mi::neuraylib::IMdl_compiler> mdl_compiler(
            neuray->get_api_component<mi::neuraylib::IMdl_compiler>());

        mi::base::Handle<mi::neuraylib::IMdl_factory> mdl_factory(
            neuray->get_api_component<mi::neuraylib::IMdl_factory>());

        mi::base::Handle < mi::neuraylib::IMdl_execution_context> context(
            mdl_factory->create_execution_context());

        // Load the module "tutorials". 
        // There is no need to configure any module search paths since
        // the mdl example folder is by default in the search path.
        check_success(mdl_compiler->load_module(
            transaction.get(), "::nvidia::sdk_examples::tutorials", context.get()) >= 0);
        print_messages(context.get());

        // get the MDLE api component
        mi::base::Handle<mi::neuraylib::IMdle_api> mdle_api(
            neuray->get_api_component<mi::neuraylib::IMdle_api>());

        // setup the export to mdle
        mi::base::Handle<mi::IStructure> data(transaction->create<mi::IStructure>("Mdle_data"));

        {
            // specify the material/function that will become the "main" of the MDLE
            mi::base::Handle<mi::IString> prototype(data->get_value<mi::IString>("prototype_name"));
            prototype->set_c_str("mdl::nvidia::sdk_examples::tutorials::example_mod_rough");
        }
        
        {
            // change a default values

            mi::base::Handle<mi::neuraylib::IType_factory> type_factory(
                mdl_factory->create_type_factory(transaction.get()));

            mi::base::Handle<mi::neuraylib::IValue_factory> value_factory(
                mdl_factory->create_value_factory(transaction.get()));

            mi::base::Handle<mi::neuraylib::IExpression_factory> expression_factory(
                mdl_factory->create_expression_factory(transaction.get()));

            // create a new set of named parameters
            mi::base::Handle<mi::neuraylib::IExpression_list> defaults(
                expression_factory->create_expression_list());

            // set a new tint value
            mi::base::Handle<mi::neuraylib::IValue_color> tint_value(
                value_factory->create_color(0.25f, 0.5f, 0.75f));

            mi::base::Handle<mi::neuraylib::IExpression_constant> tint_expr(
                expression_factory->create_constant(tint_value.get()));

            defaults->add_expression("tint", tint_expr.get());

            // set a new roughness value
            mi::base::Handle<mi::neuraylib::IValue_float> rough_value(
                value_factory->create_float(0.5f));

            mi::base::Handle<mi::neuraylib::IExpression_constant> rough_expr(
                expression_factory->create_constant(rough_value.get()));

            defaults->add_expression("roughness", rough_expr.get());

            // pass the defaults the Mdle_data struct
            data->set_value("defaults", defaults.get());
        }

        {
            // set thumbnail (files in the search paths or absolute file paths allowed as fall back)
            std::string thumbnail_path = get_samples_mdl_root()
                + "/nvidia/sdk_examples/resources/example_thumbnail.png";

            mi::base::Handle<mi::IString> thumbnail(data->get_value<mi::IString>("thumbnail_path"));
            thumbnail->set_c_str(thumbnail_path.c_str());
        }

        {
            // add additional files

            // each user file ...
            mi::base::Handle<mi::IStructure> user_file(
                transaction->create<mi::IStructure>("Mdle_user_file"));

            // ... is defined by a source path ...
            std::string readme_path = get_samples_mdl_root()
                + "/nvidia/sdk_examples/resources/example_readme.txt";

            mi::base::Handle<mi::IString> source_path(
                user_file->get_value<mi::IString>("source_path"));
            source_path->set_c_str(readme_path.c_str());

            // ... and a target path (inside the MDLE)
            mi::base::Handle<mi::IString> target_path(
                user_file->get_value<mi::IString>("target_path"));
            target_path->set_c_str("readme.txt");

            // all user files are passed as array
            mi::base::Handle<mi::IArray> user_file_array(
                transaction->create<mi::IArray>("Mdle_user_file[1]"));
            user_file_array->set_element(0, user_file.get());
            data->set_value("user_files", user_file_array.get());
        }

        // start the actual export
        const char* mdle_file_name = "example_material_blue.mdle";
        mdle_api->export_mdle(transaction.get(), mdle_file_name, data.get(), context.get());
        check_success(print_messages(context.get()));

        {
            // check and load an MDLE
            std::string mdle_path = get_working_directory() + "/" + mdle_file_name;

            // optional: check integrity of a (the created) MDLE file.
            mdle_api->validate_mdle(mdle_path.c_str(), context.get());
            check_success(print_messages(context.get()));
        
            // load the MDLE module
            mdl_compiler->load_module(transaction.get(), mdle_path.c_str(), context.get());
            check_success(print_messages(context.get()));

            // the database name begins with 'mdle::'
            // followed by the full path of the mdle file (using forward slashes) with a leading '/'
            // there is only one material/function to load, which is 'main'
            // the database name uses forward slashes
            // so, this results in:  mdle::<normalized_path>::main
            std::string main_db_name = mdle_to_db_name(mdle_path);
            std::cerr << "main_db_name: " << main_db_name.c_str() << std::endl;

            // get the main material
            mi::base::Handle<const mi::neuraylib::IMaterial_definition> material_definition(
                transaction->access<mi::neuraylib::IMaterial_definition>(main_db_name.c_str()));
            check_success(material_definition);

            // use the material ...
            std::cerr << "Successfully created and loaded " << mdle_file_name << std::endl << std::endl;

            // access the user file
            mi::base::Handle<mi::neuraylib::IReader> reader(
                mdle_api->get_user_file(mdle_path.c_str(), "readme.txt", context.get()));
            check_success(print_messages(context.get()));

            // print the content to the console
            mi::Sint64 file_size = reader->get_file_size();
            char* content = new char[file_size + 1];
            content[file_size] = '\0';
            reader->read(content, file_size);
            std::cerr << "content of the readme.txt:" << std::endl << content << std::endl << std::endl;
        }

        // ----------------------------------------------------------------------------------------

        // export a function to a second mdle
        const char* mdle_file_name2 = "example_function.mdle";
       
        {
            // setup the export to mdle
            mi::base::Handle<mi::IStructure> data(transaction->create<mi::IStructure>("Mdle_data"));

            // specify the material/function that will become the "main" of the MDLE
            mi::base::Handle<mi::IString> prototype(data->get_value<mi::IString>("prototype_name"));
            prototype->set_c_str("mdl::nvidia::sdk_examples::tutorials::example_function(color,float)");

            // start the actual export
            mdle_api->export_mdle(transaction.get(), mdle_file_name2, data.get(), context.get());
            check_success(print_messages(context.get()));
        }

        {
            // check and load the function again
            std::string mdle_path = get_working_directory() + "/" + mdle_file_name2;

            // optional: check integrity of a (the created) MDLE file.
            mdle_api->validate_mdle(mdle_path.c_str(), context.get());
            check_success(print_messages(context.get()));

            // load the MDLE module
            mdl_compiler->load_module(transaction.get(), mdle_path.c_str(), context.get());
            check_success(print_messages(context.get()));

            // the database name of functions contains the parameter list
            // therefore, the module has to be loaded first, to then get the main function name
            std::string main_db_name = mdle_to_db_name_with_signature(transaction.get(), mdle_path);
            std::cerr << "main_db_name: " << main_db_name.c_str() << std::endl;

            // get the main function
            mi::base::Handle<const mi::neuraylib::IFunction_definition> function_definition(
                transaction->access<mi::neuraylib::IFunction_definition>(main_db_name.c_str()));
            check_success(function_definition);

            // use the function ...
            std::cerr << "Successfully created and loaded " << mdle_file_name2 << std::endl << std::endl;
        }
        



        // ----------------------------------------------------------------------------------------
        // All transactions need to get committed.
        transaction->commit();
    }

    // Shut down the MDL SDK
    check_success(neuray->shutdown() == 0);
    neuray = 0;

    // Unload the MDL SDK
    check_success(unload());

    keep_console_open();
    return EXIT_SUCCESS;
}
