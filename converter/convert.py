from tinymlgen import port
import keras

if __name__ == '__main__':
    tf_model = keras.models.load_model('output/keras_model')
    c_code = port(tf_model, optimize=False)

    f = open("output/model_data.h", "w")
    f.write(c_code)
    f.close()