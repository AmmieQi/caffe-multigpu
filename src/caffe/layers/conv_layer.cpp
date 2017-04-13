#include <vector>

#include "caffe/layers/conv_layer.hpp"

namespace caffe {

template <typename Dtype>
void ConvolutionLayer<Dtype>::compute_output_shape() {
  const int* kernel_shape_data = this->kernel_shape_.cpu_data();
  const int* stride_data = this->stride_.cpu_data();
  const int* pad_data = this->pad_.cpu_data();
  const int* dilation_data = this->dilation_.cpu_data();
  this->output_shape_.clear();
  for (int i = 0; i < this->num_spatial_axes_; ++i) {
    // i + 1 to skip channel axis
    const int input_dim = this->input_shape(i + 1);
    const int kernel_extent = dilation_data[i] * (kernel_shape_data[i] - 1) + 1;
    const int output_dim = (input_dim + 2 * pad_data[i] - kernel_extent)
        / stride_data[i] + 1;
    this->output_shape_.push_back(output_dim);
  }
}

template <typename Dtype>
void ConvolutionLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
	const Dtype* weight = this->blobs_[0]->cpu_data();
	// clip by value, used in wasserstain GAN
	if (clip_by_value_){
		Dtype lower = this->layer_param_.convolution_param().clip_lower();
		Dtype upper = this->layer_param_.convolution_param().clip_upper();
		LOG(ERROR) << "before clip: ";
		for (int i = 0; i < 20; ++i){
			std::cout << this->blobs_[0]->cpu_data()[i] << " ";
		}
		std::cout << "\n";
		caffe_cpu_clip_by_value(this->blobs_[0]->count(), lower, upper, this->blobs_[0]->mutable_cpu_data());
		LOG(ERROR) << "after clip: ";
		for (int i = 0; i < 20; ++i){
			std::cout << this->blobs_[0]->cpu_data()[i] << " ";
		}
		std::cout << "\n";
		if (this->bias_term_){
			caffe_cpu_clip_by_value(this->blobs_[1]->count(), lower, upper, this->blobs_[1]->mutable_cpu_data());
		}
	}
	for (int i = 0; i < bottom.size(); ++i) {
		const Dtype* bottom_data = bottom[i]->cpu_data();
		Dtype* top_data = top[i]->mutable_cpu_data();
		for (int n = 0; n < this->num_; ++n) {
			this->forward_cpu_gemm(bottom_data + n * this->bottom_dim_, weight,
				top_data + n * this->top_dim_);
			if (this->bias_term_) {
				const Dtype* bias = this->blobs_[1]->cpu_data();
				this->forward_cpu_bias(top_data + n * this->top_dim_, bias);
			}
		}
	}
}

template <typename Dtype>
void ConvolutionLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
      const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom) {
  const Dtype* weight = this->blobs_[0]->cpu_data();
  Dtype* weight_diff = this->blobs_[0]->mutable_cpu_diff();
  bool update_weight = !this->layer_param_.convolution_param().weight_fixed();
  if (this->layer_param_.convolution_param().gen_mode() && this->gan_mode_ != 3){
	  update_weight = false;
  }
  if (this->layer_param_.convolution_param().dis_mode() && this->gan_mode_ == 3){
	  update_weight = false;
  }
  if (update_weight){
	  LOG(ERROR) << "Layer: " << this->layer_param_.name() << " update weight.";
  }
  for (int i = 0; i < top.size(); ++i) {
    const Dtype* top_diff = top[i]->cpu_diff();
    const Dtype* bottom_data = bottom[i]->cpu_data();
    Dtype* bottom_diff = bottom[i]->mutable_cpu_diff();
    // Bias gradient, if necessary.
    if (this->bias_term_ && this->param_propagate_down_[1] && update_weight) {
      Dtype* bias_diff = this->blobs_[1]->mutable_cpu_diff();
      for (int n = 0; n < this->num_; ++n) {
        this->backward_cpu_bias(bias_diff, top_diff + n * this->top_dim_);
      }
    }
    if (this->param_propagate_down_[0] || propagate_down[i]) {
      for (int n = 0; n < this->num_; ++n) {
        // gradient w.r.t. weight. Note that we will accumulate diffs.
        if (this->param_propagate_down_[0] && update_weight) {
          this->weight_cpu_gemm(bottom_data + n * this->bottom_dim_,
              top_diff + n * this->top_dim_, weight_diff);
        }
        // gradient w.r.t. bottom data, if necessary.
        if (propagate_down[i]) {
          this->backward_cpu_gemm(top_diff + n * this->top_dim_, weight,
              bottom_diff + n * this->bottom_dim_);
        }
      }
    }
  }
  // update gan_mode_
  this->gan_mode_ = this->gan_mode_ == 3 ? 1 : this->gan_mode_ + 1;
}

#ifdef CPU_ONLY
STUB_GPU(ConvolutionLayer);
#endif

INSTANTIATE_CLASS(ConvolutionLayer);

}  // namespace caffe
