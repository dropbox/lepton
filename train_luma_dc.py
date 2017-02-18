import numpy as np
import tensorflow as tf
from pydbximage.pydbximage_cpp import DbxImage

def prepare_luma_dc_dataset(files, samples_per_file=10000):
    '''
    Return a tuple of numpy arrrays (n x 319) and (n x 1) suitable for training something that maps
    8x8x5 context (minus DC) to the DC term in unquantized coefficient space.
    '''
    sample_per_file = 25000
    count = sample_per_file * len(files)
    context_deltas = [(-1,-1), (-1,0), (-1,1), (0,-1), (0,0)]
    tmp = np.zeros((count, 64*len(context_deltas)), dtype='float32')

    def get_luma_block(coeffs, qtable, index_y, index_x):
        if (index_y < 0 or index_x < 0 or
            index_y * 8 >= coeffs[0].shape[0] or
            index_x * 8 >= coeffs[0].shape[1]):
            return np.zeros((8, 8), dtype='int16')
        return coeffs[0][(index_y*8):(index_y*8+8), (index_x*8):(index_x*8+8)] * qtable

    c = 0
    for f in files:
        file_content = open(f, 'r').read()
        coeffs = DbxImage.DCT_coefficients_from_JPEG(file_content)
        qtable = np.array(DbxImage.read_JPEG_header(file_content).get('qtables')[0], dtype='int32').reshape((8, 8))

        nblocks_y = coeffs[0].shape[0] / 8
        nblocks_x = coeffs[0].shape[1] / 8
        n_blocks = nblocks_x * nblocks_y

        for _ in range(samples_per_file):
            x = int(np.random.random() * nblocks_x)
            y = int(np.random.random() * nblocks_y)
            block = [get_luma_block(coeffs, qtable, y + dy, x + dx) for (dy, dx) in context_deltas]
            tmp[c, :] = np.concatenate([np.ndarray.flatten(b) for b in block])
            c += 1

    np.random.shuffle(tmp)
    return (np.concatenate([tmp[:, :-64], tmp[:, -63:]], axis=1), tmp[:, -64:-63])


def build_and_train(input_data, output_data):
    '''
    Given n x m input and n x 1 output, train a 3-layer FCN.
    '''
    batch_size = 128
    n_training_cycles = 80000
    n_training_cycles_report = 4000
    assert output_data.shape[1] == 1
    with tf.Graph().as_default():
        c0 = input_data.shape[1]
        c1 = 128
        c2 = 32
        c3 = 1

        output = tf.placeholder(tf.float32, shape=[None, 1])
        input0 = tf.placeholder(tf.float32, shape=[None, c0])

        with tf.name_scope('fcn3'):
            fc0 = tf.Variable(tf.truncated_normal([c0,c1], stddev=1.0/8.0))
            bias0 = tf.Variable(tf.zeros([c1]))
            layer0 = tf.matmul(input0, fc0) + bias0

            fc1 = tf.Variable(tf.truncated_normal([c1,c2], stddev=1.0/8.0))
            bias1 = tf.Variable(tf.zeros([c2]))
            layer1 = tf.matmul(tf.nn.relu(layer0), fc1) + bias1

            fc2 = tf.Variable(tf.truncated_normal([c2,c3], stddev=1.0/8.0))
            bias2 = tf.Variable(tf.zeros([c3]))
            layer2 = tf.matmul(tf.nn.relu(layer1), fc2) + bias2

            error = layer2 - output
            loss = tf.reduce_mean(tf.nn.l2_loss(error))

            optimizer = tf.train.AdamOptimizer(1e-3).minimize(loss)

            session = tf.InteractiveSession()
            tf.initialize_all_variables().run()
            for step in range(n_training_cycles):
                offset = (step * batch_size) % (input_data.shape[0] - batch_size)
                batch_input = input_data[offset:(offset + batch_size), :]
                batch_output = output_data[offset:(offset + batch_size), :]
                feed_dict = {input0 : batch_input, output : batch_output}
                _, l = session.run([optimizer, loss], feed_dict=feed_dict)
                if (step + 1) % n_training_cycles_report == 0:
                    print "Minibatch loss at step %d: %f" % (step, np.mean(l) / batch_size)
                    prediction_error = session.run(error,
                                                   feed_dict= {input0 : input_data,
                                                               output : output_data})
                    print "Mean training loss %f" % (np.mean(np.square(prediction_error)))
    return fc0, bias0, fc1, bias1, fc2, bias2, fc3, bias3
