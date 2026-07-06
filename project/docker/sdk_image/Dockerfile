ARG BASE_IMAGE=ehub.cvmart.net:8443/sdk/playphonehr_24760_atlas200_a2:1.0.28.2
FROM ${BASE_IMAGE}

LABEL project="concrete_ev_sdk"
LABEL target="QA200A2-B Atlas200I A2 Ascend310B1"
LABEL description="Concrete quality detection EV_SDK algorithm image"

ENV EV_SDK_HOME=/usr/local/ev_sdk
ENV CONCRETE_MODEL_PATH=/usr/local/ev_sdk/model/concrete/best.om
ENV LD_LIBRARY_PATH=/usr/local/ev_sdk/lib:/usr/local/Ascend/driver/lib64:/usr/local/Ascend/ascend-toolkit/latest/aarch64-linux/lib64:/usr/local/Ascend/ascend-toolkit/latest/lib64:$LD_LIBRARY_PATH

RUN mkdir -p /usr/local/ev_sdk/lib \
    && mkdir -p /usr/local/ev_sdk/model/concrete \
    && mkdir -p /usr/local/ev_sdk/config \
    && mkdir -p /usr/local/ev_sdk/include \
    && mkdir -p /usr/local/ev_sdk/test

COPY build/libji.so /usr/local/ev_sdk/lib/libji.so
COPY model/concrete/best.om /usr/local/ev_sdk/model/concrete/best.om
COPY model/concrete/labels.txt /usr/local/ev_sdk/model/concrete/labels.txt
COPY include/ /usr/local/ev_sdk/include/
COPY docker/sdk_image/deps/ /usr/local/ev_sdk/lib/

COPY docker/sdk_image/algo_config.json /usr/local/ev_sdk/model/algo_config.json
COPY docker/sdk_image/algo_config.json /usr/local/ev_sdk/config/algo_config.json

COPY test_ji_calc_image.py /usr/local/ev_sdk/test/test_ji_calc_image.py
COPY test_ji_video.py /usr/local/ev_sdk/test/test_ji_video.py

RUN chmod 755 /usr/local/ev_sdk/lib/libji.so \
    && chmod 644 /usr/local/ev_sdk/model/concrete/best.om \
    && chmod 644 /usr/local/ev_sdk/model/concrete/labels.txt \
    && chmod 644 /usr/local/ev_sdk/model/algo_config.json \
    && chmod 644 /usr/local/ev_sdk/config/algo_config.json \
    && echo "===== concrete ev_sdk image files =====" \
    && ls -lh /usr/local/ev_sdk/lib/libji.so \
    && ls -lh /usr/local/ev_sdk/model/algo_config.json \
    && ls -lh /usr/local/ev_sdk/config/algo_config.json \
    && ls -lh /usr/local/ev_sdk/model/concrete/best.om \
    && ls -lh /usr/local/ev_sdk/model/concrete/labels.txt

WORKDIR /usr/local/ev_sdk
