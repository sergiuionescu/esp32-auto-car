tensorflowjs_converter --input_format=tfjs_layers_model --output_format=keras_saved_model input/actor.json output/keras_model
tflite_convert --saved_model_dir=output/keras_model --output_file=output/actor.tflite
python convert.py